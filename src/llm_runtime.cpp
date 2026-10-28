#include "resource_paths.h"
#include "llm_runtime.h"
#include "llm_channels.h"
#if defined(LIMINAL_HAVE_MLX)
#include "mlx_runtime.h"
#endif
#if defined(LIMINAL_HAVE_MLX_CPP)
#include "mlx_cpp_runtime.h"
#endif
#include <cstdlib>
#include "model_profiles.generated.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <chrono>
#include <memory>
#include <mutex>
#include <fstream>
#if defined(_WIN32)
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif
#include <thread>
#include <vector>

#include <string.h>

#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "llama.h"
#include "chat.h"
#include "common.h"
#include "sampling.h"
#include "json-schema-to-grammar.h"
#endif

namespace liminal {

namespace {
#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "sha256/sha256.c"
#endif

#if !defined(LIMINAL_LLAMA_VENDORED_COMMIT)
#define LIMINAL_LLAMA_VENDORED_COMMIT "unknown"
#endif

static std::recursive_timed_mutex g_runtime_mutex;
#if defined(LIMINAL_HAVE_LLAMA_CPP)
static bool g_llama_backend_initialized = false;
#endif
#if defined(LIMINAL_HAVE_MLX_CPP)
constexpr const char* kBackend = "mlx-cpp";
constexpr const char* kRuntimeRevision = "mlx 0.32.4 / native Gemma E4B";
#elif defined(LIMINAL_HAVE_MLX)
constexpr const char* kBackend = "mlx";
constexpr const char* kRuntimeRevision = "mlx-lm 0.32.2 / mlx 0.32.4";
#else
constexpr const char* kBackend = "llama.cpp";
constexpr const char* kRuntimeRevision = LIMINAL_LLAMA_VENDORED_COMMIT;
#endif
#if defined(LIMINAL_HAVE_LLAMA_CPP)
static std::unique_ptr<llama_model, decltype(&llama_model_free)> g_model(nullptr, llama_model_free);
static std::string g_model_path, g_model_digest;
static int g_model_gpu_layers = 0;
static int g_offloaded_layers = -1, g_total_layers = -1;
static uintmax_t g_model_size = 0;
static std::filesystem::file_time_type g_model_modified;
#endif
static thread_local LlmGenerationResult* g_active_generation = nullptr;
static thread_local LlmRuntimeTotals g_generation_totals;
using Clock = std::chrono::steady_clock;
static double ElapsedMs(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
struct GenerationTrace {
    LlmGenerationResult* result;
    const LlmGenerationConfig& config;
    Clock::time_point started = Clock::now();
    Clock::time_point inference_started{};
    bool inferring = false;
    bool cancelled = false;
    LlmStreamCallback callback = nullptr;
    void* callback_data = nullptr;
    bool count_only = false;
    bool expired() const { return ElapsedMs(started) >= config.timeout_ms; }
    bool loading_alive() {
        if (expired() || cancelled) return false;
        if (callback && !callback("", "", callback_data)) cancelled = true;
        return !cancelled;
    }
    ~GenerationTrace() {
        if(count_only) {g_active_generation=nullptr;return;}
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
                {"backend",kBackend},{"model_sha256",config.model_sha256},
                {"temperature", config.temperature}, {"seed", config.seed}, {"n_ctx", config.n_ctx},
                {"n_batch", config.n_batch}, {"n_ubatch", config.n_ubatch}, {"jinja", true},
                {"reasoning", false}, {"model_reused",result->model_reused}, {"final_budget", config.n_predict},
                {"grammar", config.use_json_grammar ? "sampling" : "off"},
                {"prompt_tokens", result->prompt_tokens}, {"generated_tokens", result->generated_tokens},
                {"reasoning_tokens", result->reasoning_tokens}, {"final_tokens", result->final_tokens},
                {"load_time_ms", result->load_time_ms}, {"wall_time_ms", result->wall_time_ms},
                {"prompt_time_ms",result->prompt_time_ms},{"generation_time_ms",result->generation_time_ms},
                {"inference_time_ms", result->inference_time_ms}, {"stop_reason", result->stop_reason},
                {"success", result->success}, {"offloaded_layers", result->offloaded_layers},
                {"total_layers", result->total_layers}, {"error", result->error_message},
                {"runtime_commit", kRuntimeRevision}
            };
            fprintf(stderr, "LIMINAL_LLM_METRIC %s\n", data.dump().c_str());
        } catch (...) { fprintf(stderr, "LLM metric serialization failed\n"); }
    }
};

#if defined(LIMINAL_HAVE_LLAMA_CPP)
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
    : n_ctx(8192), n_batch(512), n_ubatch(128),
      timeout_ms(180000), top_k(40), top_p(0.95f),
      purpose("turn"), n_threads(0), n_threads_batch(0), n_predict(768), n_gpu_layers(-1),
      seed(42u), temperature(0.0f), flash_attention(true), use_json_grammar(false)
{
#if defined(LIMINAL_HAVE_MLX)
    ConfigureLlmProfile("gemma-4-e4b-mlx-4bit", this, nullptr);
#else
    ConfigureLlmProfile(kLiminalDefaultProfile, this, nullptr);
#endif
}

bool ConfigureLlmProfile(const std::string& name, LlmGenerationConfig* config, std::string* error)
{
#if defined(LIMINAL_HAVE_MLX)
    if (config && (name == "gemma-4-e4b-mlx-4bit" || name == kLiminalDefaultProfile)) {
        config->profile_name = "gemma-4-e4b-mlx-4bit";
        const char* path = std::getenv("LIMINAL_MLX_MODEL");
        config->model_path = path ? path : LIMINAL_MLX_MODEL_DIR;
        config->model_sha256.clear(); // MLX verifies every file against models_mlx.json.
        config->temperature = 0.0f; config->top_k = 40; config->top_p = 0.95f;
        return true;
    }
#endif
#if defined(LIMINAL_HAVE_MLX_CPP)
    if (config && (name == "gemma-4-e4b-mlx-cpp-4bit" || name == kLiminalDefaultProfile)) {
        config->profile_name = "gemma-4-e4b-mlx-cpp-4bit";
        config->model_path = ResourcePath("models/mlx/gemma-4-e4b-it-4bit");
        config->model_sha256.clear(); // Embedded per-file native MLX manifest.
        config->temperature = 0.0f; config->top_k = 40; config->top_p = 0.95f;
        return true;
    }
#endif
    for (const auto& profile : kLiminalModelProfiles) {
        if (name == profile.name && config) {
            config->profile_name = name;
            config->model_path = ResourcePath(profile.path);
            config->model_sha256 = profile.sha256;
            config->temperature = profile.temperature;
            config->top_k = profile.top_k;
            config->top_p = profile.top_p;
            return true;
        }
    }
    if (error) *error = "Unknown model profile: " + name;
    return false;
}

bool InitializeLlmRuntime()
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
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
#elif defined(LIMINAL_HAVE_MLX)
    return true; // Lazy worker start on the first request.
#else
    return false;
#endif
}

void ShutdownLlmRuntime()
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
#if defined(LIMINAL_HAVE_MLX)
    ShutdownMlxRuntime();
#endif
#if defined(LIMINAL_HAVE_MLX_CPP)
    ShutdownMlxCppRuntime();
#endif
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (g_llama_backend_initialized) {
        g_model.reset();
        g_model_path.clear();
        llama_backend_free();
        g_llama_backend_initialized = false;
    }
#endif
}

bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info)
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
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

#if defined(LIMINAL_HAVE_MLX_CPP)
    info->compiled_with_mlx = true;
    info->compiled_with_llama = true; // Tokenization, Jinja and JSON grammar only.
    info->gpu_offload_supported = true;
    info->system_info = "MLX C++ 0.32.4 / Apple Metal; no Python runtime";
    return true;
#endif
    info->compiled_with_llama = true;
    info->gpu_offload_supported = llama_supports_gpu_offload();

    const char* system_info = llama_print_system_info();
    if (system_info && system_info[0]) {
        info->system_info = system_info;
    }

    return true;
#else
#if defined(LIMINAL_HAVE_MLX)
    info->compiled_with_mlx = true;
    info->gpu_offload_supported = true;
    info->system_info = "MLX-LM / Apple Metal; local worker, availability checked at first inference";
    return true;
#endif
    info->system_info = "llama.cpp support is not compiled into this binary.";
    return false;
#endif
}

static bool ChatCompletionInternal(
    const LlmGenerationConfig& requested,
    const std::vector<LlmPromptMessage>& messages,
    LlmStreamCallback stream_callback,
    void* stream_user_data,
    LlmGenerationResult* result, bool count_only)
{
    if (!result) return false;
    *result = LlmGenerationResult();
    LlmGenerationConfig config = requested;
    GenerationTrace trace{result, config};
    trace.count_only=count_only;
    trace.callback = stream_callback;
    trace.callback_data = stream_user_data;
    const auto fail = [&](const char* reason, const std::string& message) {
        result->stop_reason = reason;
        result->error_message = message;
        result->response_text.clear();
        return false;
    };
#if defined(LIMINAL_HAVE_MLX_CPP)
    if (messages.empty() || config.n_ctx < 128 || config.n_ctx > 32768 ||
        config.n_batch < 1 || config.n_batch > 32768 || config.n_predict < 1 || config.n_predict > 16384 ||
        config.timeout_ms < 1 || !std::isfinite(config.temperature) || config.temperature < 0 ||
        config.top_k < 1 || !std::isfinite(config.top_p) || config.top_p < 0 || config.top_p > 1)
        return fail("error", "Invalid generation bounds");
    if (config.n_gpu_layers != -1) return fail("error", "Native MLX uses Metal; --llm-gpu-layers is llama.cpp-only");
    std::unique_lock<std::recursive_timed_mutex> runtime_lock(g_runtime_mutex, std::defer_lock);
    while (!runtime_lock.try_lock_for(std::chrono::milliseconds(20))) {
        if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while waiting for inference");
    }
    if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped before inference");
    if (!InitializeLlmRuntime()) return fail("error", "Cannot initialize native tokenization");
    config.timeout_ms = std::max(1, config.timeout_ms - static_cast<int>(ElapsedMs(trace.started)));
    return GenerateMlxCppCompletion(config, messages, stream_callback, stream_user_data, result, count_only);
#elif defined(LIMINAL_HAVE_LLAMA_CPP)
    try {
        if (messages.empty()) return fail("error", "No messages provided");
        if (config.n_ctx < 128 || config.n_ctx > 32768 || config.n_batch < 1 || config.n_ubatch < 1 ||
            config.n_predict < 1 || config.n_predict > 16384 || config.timeout_ms < 1 ||
            !std::isfinite(config.temperature) || config.temperature < 0 ||
            config.top_k < 1 || !std::isfinite(config.top_p) || config.top_p < 0 || config.top_p > 1) {
            return fail("error", "Invalid generation bounds");
        }
        std::unique_lock<std::recursive_timed_mutex> runtime_lock(g_runtime_mutex, std::defer_lock);
        while (!runtime_lock.try_lock_for(std::chrono::milliseconds(20))) {
            if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while waiting for inference");
        }
        if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped before loading model");
        if (!InitializeLlmRuntime()) return fail("error", "Failed to initialize llama.cpp");
        g_active_generation = result;
        const auto model_path = std::filesystem::absolute(std::filesystem::u8path(config.model_path)).lexically_normal();
        if (!std::filesystem::is_regular_file(model_path)) return fail("model_missing", "Model file not found: " + config.model_path);
        const auto model_size = std::filesystem::file_size(model_path);
        const auto model_modified = std::filesystem::last_write_time(model_path);
        const bool reusable = g_model && g_model_path == model_path.u8string() &&
            g_model_gpu_layers == config.n_gpu_layers && g_model_size == model_size && g_model_modified == model_modified;
        if (!reusable) {
            g_model.reset();
            const auto load_start = Clock::now();
            std::ifstream file(model_path, std::ios::binary);
#if defined(_WIN32)
            struct HashHandles {
                BCRYPT_ALG_HANDLE algorithm = nullptr;
                BCRYPT_HASH_HANDLE hash = nullptr;
                ~HashHandles() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
            } hash;
            if (BCryptOpenAlgorithmProvider(&hash.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
                BCryptCreateHash(hash.algorithm, &hash.hash, nullptr, 0, nullptr, 0, 0) < 0)
                return fail("model_read_error", "Cannot initialize model verification");
#elif defined(__APPLE__)
            CC_SHA256_CTX hash;
            CC_SHA256_Init(&hash);
#else
            sha256_t hash;
            sha256_init(&hash);
#endif
            std::vector<unsigned char> buffer(4*1024*1024);
            while (file) {
                if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while verifying model");
                file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
#if defined(_WIN32)
                if (BCryptHashData(hash.hash, buffer.data(), static_cast<ULONG>(file.gcount()), 0) < 0)
                    return fail("model_read_error", "Cannot hash model file");
#elif defined(__APPLE__)
                CC_SHA256_Update(&hash, buffer.data(), static_cast<CC_LONG>(file.gcount()));
#else
                sha256_update(&hash, buffer.data(), static_cast<size_t>(file.gcount()));
#endif
            }
            if (!file.eof()) return fail("model_read_error", "Cannot read model file");
            unsigned char digest[SHA256_DIGEST_SIZE];
#if defined(_WIN32)
            if (BCryptFinishHash(hash.hash, digest, sizeof(digest), 0) < 0)
                return fail("model_read_error", "Cannot finalize model verification");
#elif defined(__APPLE__)
            CC_SHA256_Final(digest, &hash);
#else
            sha256_final(&hash, digest);
#endif
            g_model_digest.clear();
            static const char hex[]="0123456789abcdef";
            for (auto byte : digest) { g_model_digest += hex[byte>>4]; g_model_digest += hex[byte&15]; }
            if (!config.model_sha256.empty() && config.model_sha256 != g_model_digest)
                return fail("model_corrupt", "Model SHA-256 differs from the packaged manifest");
            llama_model_params model_params = llama_model_default_params();
            model_params.n_gpu_layers = config.n_gpu_layers;
            // The model loader invokes this callback synchronously; it is never reused
            // by inference. No context or worker retains the per-request trace.
            model_params.progress_callback = [](float, void* opaque) { return static_cast<GenerationTrace*>(opaque)->loading_alive(); };
            model_params.progress_callback_user_data = &trace;
            g_model.reset(llama_model_load_from_file(config.model_path.c_str(), model_params));
            result->load_time_ms = ElapsedMs(load_start);
            if (trace.cancelled || trace.expired()) {
                g_model.reset();
                return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while loading model");
            }
            if (!g_model) return fail("model_load_error", "Failed to load " + config.model_path);
            g_model_path = model_path.u8string();
            g_model_gpu_layers = config.n_gpu_layers;
            g_model_size = model_size;
            g_model_modified = model_modified;
            g_offloaded_layers = result->offloaded_layers;
            g_total_layers = result->total_layers;
        }
        if (!config.model_sha256.empty() && config.model_sha256 != g_model_digest)
            return fail("model_corrupt", "Resident model differs from the selected manifest");
        config.model_sha256 = g_model_digest;
        result->model_reused = reusable;
        result->offloaded_layers = g_offloaded_layers;
        result->total_layers = g_total_layers;
        auto* model = g_model.get();
        const llama_vocab* vocab = llama_model_get_vocab(model);
        common_chat_templates_ptr templates;
        std::unique_ptr<common_chat_session> chat;
        std::vector<llama_token> prompt_tokens;
        {
            templates = common_chat_templates_init(model, "");
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
            chat.reset(new common_chat_session(templates.get(), vocab, inputs));
            result->prompt_text = chat->prompt().str();
            bool first = true;
            for (const auto& part : chat->prompt().parts) {
                if (part.text.empty()) continue;
                auto tokens = common_tokenize(vocab, part.text, first, !part.is_input);
                prompt_tokens.insert(prompt_tokens.end(), tokens.begin(), tokens.end());
                first = false;
            }
        }
        result->prompt_tokens = static_cast<int>(prompt_tokens.size());
        if(count_only) {result->success=true;return true;}
        const int reserve = config.n_predict;
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
        context_params.abort_callback = [](void* opaque) { return !static_cast<GenerationTrace*>(opaque)->loading_alive(); };
        context_params.abort_callback_data = &trace;
        std::unique_ptr<llama_context, decltype(&llama_free)> context(llama_init_from_model(model, context_params), llama_free);
        if (!context) return fail("error", "Failed to create llama.cpp context");
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
        std::unique_ptr<common_sampler, decltype(&common_sampler_free)> sampler(
            common_sampler_init(model, sampling), common_sampler_free);
        if (!sampler) return fail("error", "Failed to create sampler");
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
                return fail(trace.cancelled ? "cancelled" : trace.expired() ? "timeout" : "error", "Prompt decode failed");
        }
        result->prompt_time_ms = ElapsedMs(trace.inference_started);
        const auto generation_started = Clock::now();
        std::string streamed_final;
        for (int generated = 0; generated <= reserve; ++generated) {
            if (trace.expired()) return fail("timeout", "Generation deadline exceeded");
            if (stream_callback && !stream_callback(streamed_final.c_str(), "", stream_user_data)) return fail("cancelled", "Generation cancelled");
            llama_token token = common_sampler_sample(sampler.get(), context.get(), -1);
            common_sampler_accept(sampler.get(), token, true);
            if (llama_vocab_is_eog(vocab, token)) { result->reached_eog = true; break; }
            if (generated == reserve) return fail("token_limit", "Total generation budget exhausted");
            std::string piece;
            if (!AppendTokenPiece(vocab, token, &piece, &result->error_message)) return false;
            result->raw_text += piece;
            ++result->generated_tokens;
            const auto raw_guard = GuardFinalChannel(result->raw_text, false);
            if (!raw_guard.error.empty()) return fail("protocol_error", raw_guard.error);
            ++result->final_tokens;
            common_chat_input chunk;
            chunk.append(piece, token);
            const auto& parsed = chat->feed(chunk);
            if (!parsed.reasoning_content.empty() || !parsed.tool_calls.empty()) return fail("protocol_error", "Unexpected private or tool output");
            const auto channels = GuardFinalChannel(parsed.content, false);
            if (!channels.error.empty()) return fail("protocol_error", channels.error);
            if (channels.text.compare(0, streamed_final.size(), streamed_final) != 0)
                return fail("protocol_error", "Final stream changed its prefix");
            const std::string delta = channels.text.substr(streamed_final.size());
            streamed_final = channels.text;
            if (stream_callback && !delta.empty() && !stream_callback(streamed_final.c_str(), delta.c_str(), stream_user_data))
                return fail("cancelled", "Generation cancelled");
            if (llama_decode(context.get(), llama_batch_get_one(&token, 1)) != 0)
                return fail(trace.cancelled ? "cancelled" : trace.expired() ? "timeout" : "error", "Token decode failed");
        }
        result->generation_time_ms = ElapsedMs(generation_started);
        if (!result->reached_eog) return fail("token_limit", "Missing end of generation");
        const auto raw_guard = GuardFinalChannel(result->raw_text, true);
        if (!raw_guard.error.empty()) return fail("protocol_error", raw_guard.error);
        const auto& parsed_message = chat->finish();
        if (!parsed_message.reasoning_content.empty() || !parsed_message.tool_calls.empty()) return fail("protocol_error", "Unexpected private or tool output");
        const auto channels = GuardFinalChannel(parsed_message.content, true);
        if (!channels.error.empty()) return fail("protocol_error", channels.error);
        std::string final_text = channels.text;
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
        result->stop_reason = "eos";
        result->success = true;
        return true;
    } catch (const std::exception& error) {
        return fail("error", error.what());
    }
#elif defined(LIMINAL_HAVE_MLX)
    if (messages.empty() || config.n_ctx < 128 || config.n_ctx > 32768 ||
        config.n_batch < 1 || config.n_batch > 32768 || config.n_predict < 1 || config.n_predict > 16384 ||
        config.timeout_ms < 1 || !std::isfinite(config.temperature) || config.temperature < 0 ||
        config.top_k < 1 || !std::isfinite(config.top_p) || config.top_p < 0 || config.top_p > 1)
        return fail("error", "Invalid generation bounds");
    if (config.n_gpu_layers != -1)
        return fail("error", "--llm-gpu-layers is llama.cpp-only; MLX uses Apple Metal");
    std::unique_lock<std::recursive_timed_mutex> runtime_lock(g_runtime_mutex, std::defer_lock);
    while (!runtime_lock.try_lock_for(std::chrono::milliseconds(20))) {
        if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while waiting for inference");
    }
    if (!trace.loading_alive()) return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped before MLX inference");
    config.timeout_ms = std::max(1, config.timeout_ms - static_cast<int>(ElapsedMs(trace.started)));
    return GenerateMlxCompletion(config, messages, stream_callback, stream_user_data, result, count_only);
#else
    (void) messages; (void) stream_callback; (void) stream_user_data;
    return fail("unavailable", "llama.cpp support is not compiled into this binary");
#endif
}

bool CountChatPromptTokens(const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,LlmStreamCallback callback,void* user,
    int* tokens,std::string* error) {
    if(config.prompt_token_counter)return config.prompt_token_counter(messages,tokens,error);
    LlmGenerationResult result;
    const bool ok=ChatCompletionInternal(config,messages,callback,user,&result,true);
    *tokens=result.prompt_tokens;*error=result.error_message;return ok;
}

bool GenerateChatCompletion(const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,LlmStreamCallback callback,void* user,
    LlmGenerationResult* result) {
    return ChatCompletionInternal(config,messages,callback,user,result,false);
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

    fprintf(out, "MLX compiled in: %s\n", info.compiled_with_mlx ? "yes" : "no");
    fprintf(out, "llama.cpp compiled in: %s\n", info.compiled_with_llama ? "yes" : "no");
    fprintf(out, "llama.cpp vendored commit: %s\n", info.vendored_commit ? info.vendored_commit : "unknown");
    fprintf(out, "GPU offload available: %s\n", info.gpu_offload_supported ? "yes" : "no");

    if (!info.system_info.empty()) {
        fprintf(out, "System info: %s\n", info.system_info.c_str());
    }
}

}  // namespace liminal
