#include "llm_backend.h"

#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "llama.h"
#include "chat.h"
#include "common.h"
#include "sampling.h"
#include "json-schema-to-grammar.h"
#include <algorithm>
#include <memory>
#include <thread>
#include <string.h>

#if !defined(LIMINAL_LLAMA_VENDORED_COMMIT)
#define LIMINAL_LLAMA_VENDORED_COMMIT "unknown"
#endif

namespace liminal {

namespace {

static bool g_backend_initialized = false;
static std::unique_ptr<llama_model, decltype(&llama_model_free)> g_model(nullptr, llama_model_free);
static LlmBackendModel* g_loading_model = nullptr;

struct LlamaSession {
    common_chat_templates_ptr templates;
    std::unique_ptr<common_chat_session> chat;
    std::vector<llama_token> prompt_tokens;
    size_t consumed = 0;
    std::unique_ptr<llama_context, decltype(&llama_free)> context{nullptr, llama_free};
    std::unique_ptr<common_sampler, decltype(&common_sampler_free)> sampler{nullptr, common_sampler_free};
    LlmLiveness live;
};

static LlamaSession* Cast(LlmBackendSession* session) { return reinterpret_cast<LlamaSession*>(session); }

static int ResolveThreadCount(int configured_threads)
{
    if (configured_threads > 0) {
        return configured_threads;
    }

    const unsigned int concurrency = std::thread::hardware_concurrency();
    return concurrency > 0 ? static_cast<int>(concurrency) : 4;
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

static bool Initialize(std::string*)
{
    if (!g_backend_initialized) {
        llama_log_set([](ggml_log_level level, const char* text, void*) {
            if (level == GGML_LOG_LEVEL_ERROR && text) fputs(text, stderr);
            const char* offload = text ? strstr(text, "offloaded ") : nullptr;
            if (g_loading_model && offload) {
                sscanf(offload, "offloaded %d/%d layers", &g_loading_model->offloaded_layers, &g_loading_model->total_layers);
            }
        }, 0);
        llama_backend_init();
        g_backend_initialized = true;
    }
    return true;
}

static void Shutdown()
{
    if (g_backend_initialized) {
        g_model.reset();
        llama_backend_free();
        g_backend_initialized = false;
    }
}

static void Describe(LlmRuntimeInfo* info)
{
    info->compiled_with_llama = true;
    info->vendored_commit = LIMINAL_LLAMA_VENDORED_COMMIT;
    info->gpu_offload_supported = llama_supports_gpu_offload();
    const char* system_info = llama_print_system_info();
    if (system_info && system_info[0]) info->system_info = system_info;
}

static bool LoadModel(const LlmGenerationConfig& config, LlmLiveness live, LlmBackendModel* model, std::string* error)
{
    g_model.reset();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config.n_gpu_layers;
    // The model loader invokes this callback synchronously; it is never reused
    // by inference. No context or worker retains the per-request liveness.
    model_params.progress_callback = [](float, void* opaque) { return static_cast<LlmLiveness*>(opaque)->Check(); };
    model_params.progress_callback_user_data = &live;
    g_loading_model = model;
    g_model.reset(llama_model_load_from_file(config.model_path.c_str(), model_params));
    g_loading_model = nullptr;
    if (!g_model) { *error = "Failed to load " + config.model_path; return false; }
    model->device = llama_supports_gpu_offload() ? "gpu" : "cpu";
    return true;
}

static void UnloadModel() { g_model.reset(); }

static LlmBackendSession* Open(const std::vector<LlmPromptMessage>& messages, std::string* prompt_text,
    int* prompt_tokens, std::string* error)
{
    if (!g_model) { *error = "No resident model"; return nullptr; }
    std::unique_ptr<LlamaSession> session(new LlamaSession());
    const llama_vocab* vocab = llama_model_get_vocab(g_model.get());
    session->templates = common_chat_templates_init(g_model.get(), "");
    common_chat_templates_inputs inputs;
    inputs.use_jinja = true;
    inputs.enable_thinking = false;
    inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
    for (const auto& source : messages) {
        common_chat_msg message;
        message.role = source.role;
        message.content = source.content;
        inputs.messages.push_back(message);
    }
    session->chat.reset(new common_chat_session(session->templates.get(), vocab, inputs));
    *prompt_text = session->chat->prompt().str();
    bool first = true;
    for (const auto& part : session->chat->prompt().parts) {
        if (part.text.empty()) continue;
        auto tokens = common_tokenize(vocab, part.text, first, !part.is_input);
        session->prompt_tokens.insert(session->prompt_tokens.end(), tokens.begin(), tokens.end());
        first = false;
    }
    *prompt_tokens = static_cast<int>(session->prompt_tokens.size());
    return reinterpret_cast<LlmBackendSession*>(session.release());
}

static bool Start(LlmBackendSession* handle, const LlmGenerationConfig& config, LlmLiveness live, std::string* error)
{
    LlamaSession* session = Cast(handle);
    session->live = live;
    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = config.n_ctx;
    context_params.n_batch = config.n_batch;
    context_params.n_ubatch = std::min(config.n_ubatch, config.n_batch);
    context_params.n_seq_max = 1;
    context_params.n_threads = ResolveThreadCount(config.n_threads);
    context_params.n_threads_batch = ResolveThreadCount(config.n_threads_batch);
    context_params.flash_attn_type = config.flash_attention ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
    context_params.offload_kqv = true;
    context_params.abort_callback = [](void* opaque) { return !static_cast<LlmLiveness*>(opaque)->Check(); };
    context_params.abort_callback_data = &session->live;
    session->context.reset(llama_init_from_model(g_model.get(), context_params));
    if (!session->context) { *error = "Failed to create llama.cpp context"; return false; }
    common_params_sampling sampling;
    sampling.seed = config.seed;
    sampling.temp = config.temperature;
    sampling.top_k = config.top_k;
    sampling.top_p = config.top_p;
    sampling.min_keep = 1;
    sampling.samplers = {COMMON_SAMPLER_TYPE_TOP_K, COMMON_SAMPLER_TYPE_TOP_P, COMMON_SAMPLER_TYPE_TEMPERATURE};
    if (config.use_json_grammar) {
        const auto schema = common_json::parse(config.json_schema.empty() ? R"({"type":"object"})" : config.json_schema);
        sampling.grammar = {COMMON_GRAMMAR_TYPE_OUTPUT_FORMAT, json_schema_to_grammar(schema)};
    }
    // Upstream rejection sampling checks the candidate first, and filters
    // the full vocabulary only on rejection. Constraints remain mandatory.
    session->sampler.reset(common_sampler_init(g_model.get(), sampling));
    if (!session->sampler) { *error = "Failed to create sampler"; return false; }
    return true;
}

static bool Prefill(LlmBackendSession* handle, int max_tokens, int* remaining, std::string* error)
{
    LlamaSession* session = Cast(handle);
    const size_t left = session->prompt_tokens.size() - session->consumed;
    const int count = static_cast<int>(std::min<size_t>(static_cast<size_t>(max_tokens), left));
    if (count > 0 && llama_decode(session->context.get(), llama_batch_get_one(session->prompt_tokens.data() + session->consumed, count)) != 0) {
        *error = "Prompt decode failed";
        return false;
    }
    session->consumed += static_cast<size_t>(count);
    *remaining = static_cast<int>(session->prompt_tokens.size() - session->consumed);
    return true;
}

static bool Sample(LlmBackendSession* handle, int32_t* token, bool* eog, std::string* piece, std::string* error)
{
    LlamaSession* session = Cast(handle);
    const llama_token sampled = common_sampler_sample(session->sampler.get(), session->context.get(), -1);
    common_sampler_accept(session->sampler.get(), sampled, true);
    *token = sampled;
    *eog = llama_vocab_is_eog(llama_model_get_vocab(g_model.get()), sampled);
    piece->clear();
    return *eog || AppendTokenPiece(llama_model_get_vocab(g_model.get()), sampled, piece, error);
}

static bool Feed(LlmBackendSession* handle, const std::string& piece, int32_t token, std::string* content, std::string* error)
{
    common_chat_input chunk;
    chunk.append(piece, token);
    const auto& parsed = Cast(handle)->chat->feed(chunk);
    if (!parsed.reasoning_content.empty() || !parsed.tool_calls.empty()) { *error = "Unexpected private or tool output"; return false; }
    *content = parsed.content;
    return true;
}

static bool Decode(LlmBackendSession* handle, int32_t token, std::string* error)
{
    llama_token value = token;
    if (llama_decode(Cast(handle)->context.get(), llama_batch_get_one(&value, 1)) != 0) { *error = "Token decode failed"; return false; }
    return true;
}

static bool Finish(LlmBackendSession* handle, std::string* content, std::string* error)
{
    const auto& parsed = Cast(handle)->chat->finish();
    if (!parsed.reasoning_content.empty() || !parsed.tool_calls.empty()) { *error = "Unexpected private or tool output"; return false; }
    *content = parsed.content;
    return true;
}

static void Close(LlmBackendSession* handle) { delete Cast(handle); }

static const LlmBackend kLlamaBackend = {
    "llama.cpp", LIMINAL_LLAMA_VENDORED_COMMIT, Initialize, Shutdown, Describe, LoadModel, UnloadModel,
    Open, Start, Prefill, Sample, Feed, Decode, Finish, Close,
};

}  // namespace

const LlmBackend* LlamaLlmBackend() { return &kLlamaBackend; }

}  // namespace liminal

#else

namespace liminal {
const LlmBackend* LlamaLlmBackend() { return nullptr; }
}

#endif
