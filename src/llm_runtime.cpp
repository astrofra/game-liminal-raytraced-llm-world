#include "file_io.h"
#include "llm_runtime.h"
#include "ollama_runtime.h"
#include "llm_channels.h"
#include "model_profiles.generated.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <string.h>

#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "llama.h"
#include "chat.h"
#include "common.h"
#include "json-schema-to-grammar.h"
#endif

namespace liminal {

namespace {

#if !defined(LIMINAL_LLAMA_VENDORED_COMMIT)
#define LIMINAL_LLAMA_VENDORED_COMMIT "unknown"
#endif

static bool g_llama_backend_initialized = false;
static thread_local LlmGenerationResult* g_active_generation = nullptr;
static thread_local LlmRuntimeTotals g_generation_totals;
using Clock = std::chrono::steady_clock;
static double ElapsedMs(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
struct GenerationTrace {
    LlmGenerationResult* result;
    const LlmGenerationConfig& config;
    Clock::time_point started = Clock::now();
    Clock::time_point inference_started;
    bool inferring = false;
    bool cancelled = false;
    LlmStreamCallback callback = nullptr;
    void* callback_data = nullptr;
    bool expired() const { return ElapsedMs(started) >= config.timeout_ms; }
    bool loading_alive() {
        if (expired() || cancelled) return false;
        if (callback && !callback("", "", callback_data)) cancelled = true;
        return !cancelled;
    }
    ~GenerationTrace() {
        result->wall_time_ms = ElapsedMs(started);
        if (inferring) result->inference_time_ms = ElapsedMs(inference_started);
        if (result->stop_reason.empty()) result->stop_reason = "error";
        ++g_generation_totals.calls;
        g_generation_totals.prompt_tokens += result->prompt_tokens;
        g_generation_totals.generated_tokens += result->generated_tokens;
        g_generation_totals.reasoning_tokens += result->reasoning_tokens;
        g_generation_totals.final_tokens += result->final_tokens;
        g_generation_totals.inference_time_ms += result->inference_time_ms;
        g_generation_totals.wall_time_ms += result->wall_time_ms;
        g_active_generation = nullptr;
        try {
            nlohmann::json data = {
                {"profile", config.profile_name}, {"model_path", config.model_path}, {"purpose", config.purpose},
                {"backend",config.use_ollama?"ollama":"llama.cpp"},{"ollama_model",config.use_ollama?config.ollama_model:""},
                {"temperature", config.temperature}, {"seed", config.seed}, {"n_ctx", config.n_ctx},
                {"n_batch", config.n_batch}, {"n_ubatch", config.n_ubatch}, {"jinja", config.use_jinja},
                {"reasoning", config.reasoning}, {"reasoning_budget", config.reasoning_budget}, {"final_budget", config.n_predict},
                {"grammar", config.use_json_grammar ? (config.reasoning ? "final_validation" : "sampling") : "off"},
                {"prompt_tokens", result->prompt_tokens}, {"generated_tokens", result->generated_tokens},
                {"reasoning_tokens", result->reasoning_tokens}, {"final_tokens", result->final_tokens},
                {"load_time_ms", result->load_time_ms}, {"wall_time_ms", result->wall_time_ms},
                {"prompt_time_ms",result->prompt_time_ms},{"generation_time_ms",result->generation_time_ms},
                {"inference_time_ms", result->inference_time_ms}, {"stop_reason", result->stop_reason},
                {"success", result->success}, {"offloaded_layers", result->offloaded_layers},
                {"total_layers", result->total_layers}, {"error", result->error_message},
                {"runtime_commit", LIMINAL_LLAMA_VENDORED_COMMIT}
            };
            fprintf(stderr, "LIMINAL_LLM_METRIC %s\n", data.dump().c_str());
        } catch (...) { fprintf(stderr, "LLM metric serialization failed\n"); }
    }
};

#if defined(LIMINAL_HAVE_LLAMA_CPP)
static bool ReadTextFile(const char* path, std::string* text)
{
    if (!path || !text) {
        return false;
    }

    FILE* file = liminal::OpenUtf8File(path, "rb");
    if (!file) {
        return false;
    }

    text->clear();
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        text->append(buffer);
    }

    fclose(file);
    return true;
}

static bool ApplyChatTemplate(
    const llama_model* model,
    const std::vector<LlmPromptMessage>& messages,
    std::string* prompt_text,
    std::string* error_message)
{
    if (!model || !prompt_text) {
        if (error_message) {
            *error_message = "Invalid model or prompt buffer.";
        }
        return false;
    }

    std::vector<llama_chat_message> chat;
    size_t reserve_size = 0;
    chat.reserve(messages.size());

    for (size_t index = 0; index < messages.size(); ++index) {
        const LlmPromptMessage& message = messages[index];
        chat.push_back(llama_chat_message{message.role.c_str(), message.content.c_str()});
        reserve_size += message.role.size() + message.content.size();
    }

    const char* template_text = llama_model_chat_template(model, 0);
    if (!template_text || !template_text[0]) {
        prompt_text->clear();
        for (size_t index = 0; index < messages.size(); ++index) {
            prompt_text->append(messages[index].role);
            prompt_text->append(":\n");
            prompt_text->append(messages[index].content);
            prompt_text->append("\n\n");
        }
        prompt_text->append("assistant:\n");
        return true;
    }

    std::vector<char> buffer(std::max<size_t>(2048, reserve_size * 2 + 256));
    int32_t result = llama_chat_apply_template(template_text, chat.data(), chat.size(), true, buffer.data(), static_cast<int32_t>(buffer.size()));
    if (result < 0) {
        if (error_message) {
            *error_message = "Chat template application failed.";
        }
        return false;
    }

    if (static_cast<size_t>(result) > buffer.size()) {
        buffer.resize(result);
        result = llama_chat_apply_template(template_text, chat.data(), chat.size(), true, buffer.data(), static_cast<int32_t>(buffer.size()));
    }

    if (result < 0 || static_cast<size_t>(result) > buffer.size()) {
        if (error_message) {
            *error_message = "Chat template application produced an invalid size.";
        }
        return false;
    }

    prompt_text->assign(buffer.data(), result);
    return true;
}

static bool TokenizePrompt(
    const llama_vocab* vocab,
    const std::string& prompt_text,
    std::vector<llama_token>* tokens,
    std::string* error_message)
{
    if (!vocab || !tokens) {
        if (error_message) {
            *error_message = "Invalid tokenizer inputs.";
        }
        return false;
    }

    const int32_t required = llama_tokenize(
        vocab,
        prompt_text.c_str(),
        static_cast<int32_t>(prompt_text.size()),
        0,
        0,
        true,
        true);
    if (required >= 0) {
        if (error_message) {
            *error_message = "Unexpected tokenizer size response.";
        }
        return false;
    }

    tokens->resize(static_cast<size_t>(-required));
    const int32_t written = llama_tokenize(
        vocab,
        prompt_text.c_str(),
        static_cast<int32_t>(prompt_text.size()),
        tokens->data(),
        static_cast<int32_t>(tokens->size()),
        true,
        true);
    if (written < 0) {
        if (error_message) {
            *error_message = "Tokenizer failed while writing tokens.";
        }
        return false;
    }

    tokens->resize(static_cast<size_t>(written));
    return true;
}

static bool AppendTokenPiece(const llama_vocab* vocab, llama_token token, std::string* output_text, std::string* error_message)
{
    if (!vocab || !output_text) {
        if (error_message) {
            *error_message = "Invalid detokenization inputs.";
        }
        return false;
    }

    char local_buffer[256];
    int32_t piece_size = llama_token_to_piece(vocab, token, local_buffer, static_cast<int32_t>(sizeof(local_buffer)), 0, true);
    if (piece_size < 0) {
        std::vector<char> dynamic_buffer(static_cast<size_t>(-piece_size));
        piece_size = llama_token_to_piece(vocab, token, dynamic_buffer.data(), static_cast<int32_t>(dynamic_buffer.size()), 0, true);
        if (piece_size < 0) {
            if (error_message) {
                *error_message = "Detokenization failed.";
            }
            return false;
        }
        output_text->append(dynamic_buffer.data(), piece_size);
        return true;
    }

    output_text->append(local_buffer, piece_size);
    return true;
}

static int ResolveThreadCount(int configured_threads)
{
    if (configured_threads > 0) {
        return configured_threads;
    }

    const unsigned int concurrency = std::thread::hardware_concurrency();
    return concurrency > 0 ? static_cast<int>(concurrency) : 4;
}
#endif

}  // namespace

LlmRuntimeTotals QueryLlmRuntimeTotals() { return g_generation_totals; }

LlmGenerationConfig::LlmGenerationConfig()
    : grammar_path("vendor/llama.cpp/grammars/json.gbnf"), n_ctx(8192), n_batch(512), n_ubatch(128),
      reasoning_budget(512), timeout_ms(180000), top_k(40), top_p(0.95f), reasoning(false), use_jinja(true),
      short_call(false), purpose("turn"), n_threads(0), n_threads_batch(0), n_predict(768), n_gpu_layers(-1),
      seed(42u), temperature(0.0f), flash_attention(true), use_json_grammar(false)
{
    ConfigureLlmProfile(kLiminalDefaultProfile, this, nullptr);
}

bool ConfigureLlmProfile(const std::string& name, LlmGenerationConfig* config, std::string* error)
{
    for (const auto& profile : kLiminalModelProfiles) {
        if (name == profile.name && config) {
            config->profile_name = name;
            config->model_path = profile.path;
            config->reasoning = profile.reasoning;
            config->temperature = profile.temperature;
            config->top_k = profile.top_k;
            config->top_p = profile.top_p;
            return true;
        }
    }
    if (error) *error = "Unknown Ministral profile: " + name;
    return false;
}

bool InitializeLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!g_llama_backend_initialized) {
        llama_log_set([](ggml_log_level level, const char* text, void*) {
            if (level == GGML_LOG_LEVEL_ERROR && text) fputs(text, stderr);
            const char* offload = text ? strstr(text, "offloaded ") : nullptr;
            if (g_active_generation && offload) {
                sscanf(offload, "offloaded %d/%d layers", &g_active_generation->offloaded_layers, &g_active_generation->total_layers);
            }
        }, 0);
        llama_backend_init();
        g_llama_backend_initialized = true;
    }
    return true;
#else
    return false;
#endif
}

void ShutdownLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (g_llama_backend_initialized) {
        llama_backend_free();
        g_llama_backend_initialized = false;
    }
#endif
}

bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info)
{
    if (!info) {
        return false;
    }

    info->compiled_with_llama = false;
    info->gpu_offload_supported = false;
    info->vendored_commit = LIMINAL_LLAMA_VENDORED_COMMIT;
    info->system_info.clear();

#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!InitializeLlmRuntime()) {
        return false;
    }

    info->compiled_with_llama = true;
    info->gpu_offload_supported = llama_supports_gpu_offload();

    const char* system_info = llama_print_system_info();
    if (system_info && system_info[0]) {
        info->system_info = system_info;
    }

    return true;
#else
    info->system_info = "llama.cpp support is not compiled into this binary.";
    return false;
#endif
}

bool GenerateChatCompletion(
    const LlmGenerationConfig& requested,
    const std::vector<LlmPromptMessage>& messages,
    LlmStreamCallback stream_callback,
    void* stream_user_data,
    LlmGenerationResult* result)
{
    if (!result) return false;
    *result = LlmGenerationResult();
    LlmGenerationConfig config = requested;
    // Auxiliary calls use the Instruct checkpoint, never a fictitious reasoning-off mode.
    if (!config.use_ollama && config.reasoning && config.short_call) {
        const float modulation = config.temperature - 0.7f;
        ConfigureLlmProfile("ministral-8b-q4", &config, nullptr);
        config.temperature = std::max(0.0f, 0.1f + modulation);
    }
    GenerationTrace trace{result, config};
    trace.callback = stream_callback;
    trace.callback_data = stream_user_data;
    if(config.use_ollama)return GenerateOllamaCompletion(config,messages,stream_callback,stream_user_data,result);
    const auto fail = [&](const char* reason, const std::string& message) {
        result->stop_reason = reason;
        result->error_message = message;
        result->response_text.clear();
        return false;
    };
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    try {
        if (messages.empty()) return fail("error", "No messages provided");
        if (config.n_ctx < 128 || config.n_ctx > 32768 || config.n_batch < 1 || config.n_ubatch < 1 ||
            config.n_predict < 1 || config.n_predict > 16384 || config.timeout_ms < 1 ||
            !std::isfinite(config.temperature) || config.temperature < 0 ||
            (config.reasoning && (config.reasoning_budget < 1 || config.reasoning_budget > 16384 || !config.use_jinja))) {
            return fail("error", "Invalid generation bounds or reasoning without Jinja");
        }
        if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped before loading model");
        if (!InitializeLlmRuntime()) return fail("error", "Failed to initialize llama.cpp");
        g_active_generation = result;
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = config.n_gpu_layers;
        model_params.progress_callback = [](float, void* opaque) { return static_cast<GenerationTrace*>(opaque)->loading_alive(); };
        model_params.progress_callback_user_data = &trace;
        std::unique_ptr<llama_model, decltype(&llama_model_free)> model(
            llama_model_load_from_file(config.model_path.c_str(), model_params), llama_model_free);
        result->load_time_ms = ElapsedMs(trace.started);
        if (trace.cancelled) return fail("cancelled", "Cancelled while loading model");
        if (trace.expired()) return fail("timeout", "Deadline exceeded while loading model");
        if (!model) return fail("error", "Failed to load " + config.model_path);
        const llama_vocab* vocab = llama_model_get_vocab(model.get());
        common_chat_templates_ptr templates;
        std::unique_ptr<common_chat_session> chat;
        std::string generation_prefix;
        std::vector<llama_token> prompt_tokens;
        if (config.use_jinja) {
            templates = common_chat_templates_init(model.get(), "");
            common_chat_templates_inputs inputs;
            inputs.use_jinja = true;
            inputs.enable_thinking = config.reasoning;
            inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
            for (const auto& source : messages) {
                common_chat_msg message;
                message.role = source.role;
                message.content = source.content;
                if (config.reasoning && source.role == "system") message.content += "\nReason privately inside [THINK] and [/THINK]. All output-format requirements above apply only to the final answer after [/THINK].";
                inputs.messages.push_back(message);
            }
            chat.reset(new common_chat_session(templates.get(), vocab, inputs));
            result->prompt_text = chat->prompt().str();
            generation_prefix = chat->generation_prompt();
            bool first = true;
            for (const auto& part : chat->prompt().parts) {
                if (part.text.empty()) continue;
                auto tokens = common_tokenize(vocab, part.text, first, !part.is_input);
                prompt_tokens.insert(prompt_tokens.end(), tokens.begin(), tokens.end());
                first = false;
            }
        } else {
            if (!ApplyChatTemplate(model.get(), messages, &result->prompt_text, &result->error_message) ||
                !TokenizePrompt(vocab, result->prompt_text, &prompt_tokens, &result->error_message)) return false;
        }
        result->prompt_tokens = static_cast<int>(prompt_tokens.size());
        const int reserve = config.n_predict + (config.reasoning ? config.reasoning_budget : 0);
        if (result->prompt_tokens + reserve + 1 > config.n_ctx) return fail("context_limit", "Prompt and reserved output exceed fixed context budget");
        llama_context_params context_params = llama_context_default_params();
        context_params.n_ctx = config.n_ctx;
        context_params.n_batch = config.n_batch;
        context_params.n_ubatch = std::min(config.n_ubatch, config.n_batch);
        context_params.n_seq_max = 1;
        context_params.n_threads = ResolveThreadCount(config.n_threads);
        context_params.n_threads_batch = ResolveThreadCount(config.n_threads_batch);
        context_params.flash_attn_type = config.flash_attention ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
        context_params.offload_kqv = true;
        context_params.abort_callback = [](void* opaque) { return static_cast<GenerationTrace*>(opaque)->expired(); };
        context_params.abort_callback_data = &trace;
        std::unique_ptr<llama_context, decltype(&llama_free)> context(llama_init_from_model(model.get(), context_params), llama_free);
        if (!context) return fail("error", "Failed to create llama.cpp context");
        std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler(
            llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free);
        if (!sampler) return fail("error", "Failed to create sampler");
        if (config.use_json_grammar && !config.reasoning) {
            std::string grammar;
            if (!config.json_schema.empty()) grammar = json_schema_to_grammar(common_json::parse(config.json_schema));
            else if (!ReadTextFile(config.grammar_path.c_str(), &grammar) || grammar.empty()) return fail("error", "JSON grammar unavailable");
            auto grammar_sampler = llama_sampler_init_grammar(vocab, grammar.c_str(), "root");
            if (!grammar_sampler) return fail("error", "JSON grammar initialization failed");
            llama_sampler_chain_add(sampler.get(), grammar_sampler);
        }
        if (config.temperature <= 0) llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
        else {
            llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(config.top_k));
            llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(config.top_p, 1));
            llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(config.temperature));
            llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(config.seed));
        }
        trace.inference_started = Clock::now();
        trace.inferring = true;
        struct InferenceEnd {
            GenerationTrace& trace;
            ~InferenceEnd() {
                trace.result->inference_time_ms = ElapsedMs(trace.inference_started);
                trace.inferring = false;
            }
        } inference_end{trace};
        for (size_t offset = 0; offset < prompt_tokens.size(); offset += config.n_batch) {
            if (trace.expired()) return fail("timeout", "Prompt deadline exceeded");
            if (stream_callback && !stream_callback("", "", stream_user_data)) return fail("cancelled", "Cancelled before generation");
            const int count = static_cast<int>(std::min<size_t>(config.n_batch, prompt_tokens.size() - offset));
            if (llama_decode(context.get(), llama_batch_get_one(prompt_tokens.data() + offset, count)) != 0)
                return fail(trace.expired() ? "timeout" : "error", "Prompt decode failed");
        }
        std::string streamed_final;
        for (int generated = 0; generated <= reserve; ++generated) {
            if (trace.expired()) return fail("timeout", "Generation deadline exceeded");
            if (stream_callback && !stream_callback(streamed_final.c_str(), "", stream_user_data)) return fail("cancelled", "Generation cancelled");
            llama_token token = llama_sampler_sample(sampler.get(), context.get(), -1);
            if (llama_vocab_is_eog(vocab, token)) { result->reached_eog = true; break; }
            if (generated == reserve) return fail("token_limit", "Total generation budget exhausted");
            std::string piece;
            if (!AppendTokenPiece(vocab, token, &piece, &result->error_message)) return false;
            result->raw_text += piece;
            ++result->generated_tokens;
            auto channels = InspectMinistralChannels(result->raw_text, generation_prefix, config.reasoning, false);
            if (!channels.error.empty()) return fail("protocol_error", channels.error);
            result->reasoning_text = channels.thought;
            if (channels.thinking || (channels.has_reasoning && channels.final_text.empty())) ++result->reasoning_tokens;
            else ++result->final_tokens;
            if (result->reasoning_tokens > config.reasoning_budget && config.reasoning) return fail("reasoning_limit", "Thought budget exhausted before final answer");
            if (result->final_tokens > config.n_predict) return fail("final_limit", "Final answer budget exhausted");
            if (chat) {
                common_chat_input chunk;
                chunk.append(piece, token);
                chat->feed(chunk);
            }
            if (channels.final_text.compare(0, streamed_final.size(), streamed_final) != 0)
                return fail("protocol_error", "Final stream changed its prefix");
            const std::string delta = channels.final_text.substr(streamed_final.size());
            streamed_final = channels.final_text;
            if (stream_callback && !delta.empty() && !stream_callback(streamed_final.c_str(), delta.c_str(), stream_user_data))
                return fail("cancelled", "Generation cancelled");
            if (llama_decode(context.get(), llama_batch_get_one(&token, 1)) != 0)
                return fail(trace.expired() ? "timeout" : "error", "Token decode failed");
        }
        const auto channels = InspectMinistralChannels(result->raw_text, generation_prefix, config.reasoning, true);
        if (!result->reached_eog) return fail("token_limit", "Missing end of generation");
        if (!channels.error.empty()) return fail("protocol_error", channels.error);
        std::string final_text = channels.final_text;
        if (chat) {
            const auto& parsed = chat->finish();
            // The vendor parser is authoritative; the independent guard above prevents
            // its partial-output recovery from admitting an unfinished thought.
            final_text = parsed.content;
            if (final_text.find_first_not_of(" \r\n\t") == std::string::npos) return fail("protocol_error", "Template parser returned no final content");
            const auto final_guard = InspectMinistralChannels(final_text, "", false, true);
            if (!final_guard.error.empty()) return fail("protocol_error", final_guard.error);
        }
        if (config.use_json_grammar) {
            const size_t begin = final_text.find_first_not_of(" \r\n\t");
            if (begin != std::string::npos && final_text.compare(begin, 3, "```") == 0) {
                const size_t line = final_text.find('\n', begin);
                const size_t end = final_text.rfind("```");
                if (line != std::string::npos && end > line) final_text = final_text.substr(line + 1, end - line - 1);
            }
            const auto parsed = nlohmann::json::parse(final_text, nullptr, false);
            if (parsed.is_discarded() || !parsed.is_object()) return fail("invalid_final_json", "Final channel is not a JSON object");
        }
        if (trace.expired()) return fail("timeout", "Deadline exceeded during final validation");
        if (stream_callback && !stream_callback(streamed_final.c_str(), "", stream_user_data))
            return fail("cancelled", "Cancelled before publishing final answer");
        result->response_text = final_text;
        result->reasoning_text = channels.thought;
        result->stop_reason = "eos";
        result->success = true;
        return true;
    } catch (const std::exception& error) {
        return fail("error", error.what());
    }
#else
    (void) messages; (void) stream_callback; (void) stream_user_data;
    return fail("unavailable", "llama.cpp support is not compiled into this binary");
#endif
}

bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmGenerationResult* result)
{
    return GenerateChatCompletion(config, messages, 0, 0, result);
}

void PrintLlmRuntimeInfo(FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    LlmRuntimeInfo info;
    QueryLlmRuntimeInfo(&info);

    fprintf(out, "llama.cpp compiled in: %s\n", info.compiled_with_llama ? "yes" : "no");
    fprintf(out, "llama.cpp vendored commit: %s\n", info.vendored_commit ? info.vendored_commit : "unknown");
    fprintf(out, "GPU offload available: %s\n", info.gpu_offload_supported ? "yes" : "no");

    if (!info.system_info.empty()) {
        fprintf(out, "System info: %s\n", info.system_info.c_str());
    }
}

}  // namespace liminal
