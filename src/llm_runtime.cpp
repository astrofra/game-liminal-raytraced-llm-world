#include "resource_paths.h"
#include "llm_runtime.h"
#include "llm_backend.h"
#include "llm_channels.h"
#include "model_profiles.generated.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif
#include <vector>

#include <string.h>

namespace liminal {

namespace {
#if !defined(_WIN32)
#include "sha256/sha256.c"
#endif

static std::recursive_timed_mutex g_runtime_mutex;
static const LlmBackend* g_backend = nullptr;
static bool g_backend_initialized = false;
static const LlmBackend* g_model_backend = nullptr;
static std::string g_model_path, g_model_digest;
static int g_model_gpu_layers = 0;
static LlmBackendModel g_model_info;
static uintmax_t g_model_size = 0;
static std::filesystem::file_time_type g_model_modified;
static thread_local LlmRuntimeTotals g_generation_totals;
using Clock = std::chrono::steady_clock;
static double ElapsedMs(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

static bool ResolveBackend(const std::string& name, const LlmBackend** backend, std::string* error)
{
    if (name.empty() || name == "llama" || name == "llama.cpp") {
        *backend = LlamaLlmBackend();
        if (!*backend) { *error = "llama.cpp support is not compiled into this binary"; return false; }
        return true;
    }
    if (name == "native" || name == "cpu-ref" || name == "mlx") {
        *backend = NativeLlmBackend(name);
        if (*backend) return true;
    }
    *error = "LLM backend '" + name + "' is not available in this binary";
    return false;
}

// Process default: LIMINAL_LLM_BACKEND, else llama.cpp. Returns nullptr with an error when unavailable.
static const LlmBackend* ActiveBackend(std::string* error)
{
    if (!g_backend) {
        const char* requested = std::getenv("LIMINAL_LLM_BACKEND");
        ResolveBackend(requested ? requested : "", &g_backend, error);
    }
    return g_backend;
}

// LIMINAL_LLM_CAPTURE_DIR records every real call so a native backend can be
// compared against the same prompts; the game itself never reads it back.
static void CaptureCall(const LlmGenerationConfig& config, const std::vector<LlmPromptMessage>& messages,
    const LlmGenerationResult& result, const char* backend)
{
    const char* directory = std::getenv("LIMINAL_LLM_CAPTURE_DIR");
    if (!directory || !directory[0]) return;
    static std::atomic<unsigned> sequence{0};
    static const long long process_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    nlohmann::json data = {
        {"backend", backend}, {"purpose", config.purpose}, {"profile", config.profile_name},
        {"temperature", config.temperature}, {"top_k", config.top_k}, {"top_p", config.top_p}, {"seed", config.seed},
        {"n_ctx", config.n_ctx}, {"n_predict", config.n_predict}, {"use_json_grammar", config.use_json_grammar},
        {"json_schema", config.json_schema}, {"messages", nlohmann::json::array()},
        {"prompt_text", result.prompt_text}, {"prompt_tokens", result.prompt_tokens},
        {"raw_text", result.raw_text}, {"response_text", result.response_text},
        {"stop_reason", result.stop_reason}, {"success", result.success},
        {"generated_tokens", result.generated_tokens}, {"prompt_time_ms", result.prompt_time_ms},
        {"generation_time_ms", result.generation_time_ms}
    };
    for (const auto& message : messages) data["messages"].push_back({{"role", message.role}, {"content", message.content}});
    std::string purpose = config.purpose;
    for (auto& c : purpose) if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
    char name[96];
    snprintf(name, sizeof(name), "%lld-%04u-", process_stamp, sequence++);
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::u8path(directory), ignored);
    std::ofstream out(std::filesystem::u8path(directory) / (name + purpose + ".json"), std::ios::binary);
    out << data.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << "\n";
}

struct GenerationTrace {
    LlmGenerationResult* result;
    const LlmGenerationConfig& config;
    const std::vector<LlmPromptMessage>& messages;
    Clock::time_point started = Clock::now();
    Clock::time_point inference_started;
    bool inferring = false;
    bool cancelled = false;
    LlmStreamCallback callback = nullptr;
    void* callback_data = nullptr;
    bool count_only = false;
    const LlmBackend* backend = nullptr;
    bool expired() const { return ElapsedMs(started) >= config.timeout_ms; }
    bool loading_alive() {
        if (expired() || cancelled) return false;
        if (callback && !callback("", "", callback_data)) cancelled = true;
        return !cancelled;
    }
    LlmLiveness liveness() {
        return {[](void* opaque) { return static_cast<GenerationTrace*>(opaque)->loading_alive(); }, this};
    }
    ~GenerationTrace() {
        if(count_only) return;
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
        const char* backend_name = backend ? backend->name : "none";
        try {
            nlohmann::json data = {
                {"profile", config.profile_name}, {"model_path", config.model_path}, {"purpose", config.purpose},
                {"backend", backend_name}, {"device", g_model_info.device}, {"model_sha256",config.model_sha256},
                {"temperature", config.temperature}, {"seed", config.seed}, {"n_ctx", config.n_ctx},
                {"n_batch", config.n_batch}, {"n_ubatch", config.n_ubatch}, {"jinja", true},
                {"reasoning", false}, {"model_reused",result->model_reused}, {"final_budget", config.n_predict},
                {"grammar", config.use_json_grammar ? "sampling" : "off"},
                {"prompt_tokens", result->prompt_tokens}, {"generated_tokens", result->generated_tokens},
                {"prefix_reused_tokens", 0},
                {"reasoning_tokens", result->reasoning_tokens}, {"final_tokens", result->final_tokens},
                {"load_time_ms", result->load_time_ms}, {"wall_time_ms", result->wall_time_ms},
                {"prompt_time_ms",result->prompt_time_ms},{"generation_time_ms",result->generation_time_ms},
                {"inference_time_ms", result->inference_time_ms}, {"stop_reason", result->stop_reason},
                {"success", result->success}, {"offloaded_layers", result->offloaded_layers},
                {"total_layers", result->total_layers}, {"error", result->error_message},
                {"runtime_commit", backend ? backend->commit : "none"}
            };
            fprintf(stderr, "LIMINAL_LLM_METRIC %s\n", data.dump().c_str());
            CaptureCall(config, messages, *result, backend_name);
        } catch (...) { fprintf(stderr, "LLM metric serialization failed\n"); }
    }
};

struct SessionCloser {
    const LlmBackend* backend;
    LlmBackendSession* session;
    ~SessionCloser() { if (session) backend->close(session); }
};

// Streams the whole file once; returns the lowercase hex digest.
static bool HashModelFile(const std::filesystem::path& model_path, GenerationTrace& trace, std::string* digest_hex,
    const char** reason, std::string* error)
{
    std::ifstream file(model_path, std::ios::binary);
#if defined(_WIN32)
    struct HashHandles {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        ~HashHandles() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } hash;
    if (BCryptOpenAlgorithmProvider(&hash.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(hash.algorithm, &hash.hash, nullptr, 0, nullptr, 0, 0) < 0) {
        *reason = "model_read_error"; *error = "Cannot initialize model verification"; return false;
    }
#else
    sha256_t hash;
    sha256_init(&hash);
#endif
    std::vector<unsigned char> buffer(4*1024*1024);
    while (file) {
        if (!trace.loading_alive()) {
            *reason = trace.cancelled ? "cancelled" : "timeout"; *error = "Stopped while verifying model"; return false;
        }
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
#if defined(_WIN32)
        if (BCryptHashData(hash.hash, buffer.data(), static_cast<ULONG>(file.gcount()), 0) < 0) {
            *reason = "model_read_error"; *error = "Cannot hash model file"; return false;
        }
#else
        sha256_update(&hash, buffer.data(), static_cast<size_t>(file.gcount()));
#endif
    }
    if (!file.eof()) { *reason = "model_read_error"; *error = "Cannot read model file"; return false; }
    unsigned char digest[32];
#if defined(_WIN32)
    if (BCryptFinishHash(hash.hash, digest, sizeof(digest), 0) < 0) {
        *reason = "model_read_error"; *error = "Cannot finalize model verification"; return false;
    }
#else
    sha256_final(&hash, digest);
#endif
    digest_hex->clear();
    static const char hex[]="0123456789abcdef";
    for (auto byte : digest) { *digest_hex += hex[byte>>4]; *digest_hex += hex[byte&15]; }
    return true;
}

}  // namespace

LlmRuntimeTotals QueryLlmRuntimeTotals() { return g_generation_totals; }

LlmGenerationConfig::LlmGenerationConfig()
    : n_ctx(8192), n_batch(512), n_ubatch(128),
      timeout_ms(180000), top_k(40), top_p(0.95f),
      purpose("turn"), n_threads(0), n_threads_batch(0), n_predict(768), n_gpu_layers(-1),
      seed(42u), temperature(0.0f), flash_attention(true), use_json_grammar(false)
{
    ConfigureLlmProfile(kLiminalDefaultProfile, this, nullptr);
}

bool ConfigureLlmProfile(const std::string& name, LlmGenerationConfig* config, std::string* error)
{
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

bool SelectLlmBackend(const std::string& name, std::string* error)
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
    const LlmBackend* backend = nullptr;
    std::string message;
    if (!ResolveBackend(name, &backend, &message)) { if (error) *error = message; return false; }
    if (backend != g_backend) {
        ShutdownLlmRuntime();
        g_backend = backend;
    }
    return true;
}

bool InitializeLlmRuntime()
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
    std::string error;
    const LlmBackend* backend = ActiveBackend(&error);
    if (!backend) return false;
    if (!g_backend_initialized) {
        if (!backend->initialize(&error)) return false;
        g_backend_initialized = true;
    }
    return true;
}

void ShutdownLlmRuntime()
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
    if (g_backend && g_backend_initialized) {
        g_backend->unload_model();
        g_backend->shutdown();
        g_backend_initialized = false;
    }
    g_model_backend = nullptr;
    g_model_path.clear();
}

bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info)
{
    std::lock_guard<std::recursive_timed_mutex> lock(g_runtime_mutex);
    if (!info) {
        return false;
    }

    info->compiled_with_llama = LlamaLlmBackend() != nullptr;
    info->gpu_offload_supported = false;
    info->vendored_commit = "not-vendored";
    info->system_info.clear();

    std::string error;
    const LlmBackend* backend = ActiveBackend(&error);
    if (!backend || !InitializeLlmRuntime()) {
        info->system_info = error.empty() ? "LLM backend initialization failed." : error;
        return false;
    }
    backend->describe(info);
    return true;
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
    GenerationTrace trace{result, config, messages};
    trace.count_only=count_only;
    trace.callback = stream_callback;
    trace.callback_data = stream_user_data;
    const auto fail = [&](const char* reason, const std::string& message) {
        result->stop_reason = reason;
        result->error_message = message;
        result->response_text.clear();
        return false;
    };
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
        std::string backend_error;
        const LlmBackend* backend = ActiveBackend(&backend_error);
        if (!backend) return fail("unavailable", backend_error);
        trace.backend = backend;
        if (!InitializeLlmRuntime()) return fail("error", std::string("Failed to initialize ") + backend->name);
        const auto model_path = std::filesystem::absolute(std::filesystem::u8path(config.model_path)).lexically_normal();
        if (!std::filesystem::is_regular_file(model_path)) return fail("model_missing", "Model file not found: " + config.model_path);
        const auto model_size = std::filesystem::file_size(model_path);
        const auto model_modified = std::filesystem::last_write_time(model_path);
        const bool reusable = g_model_backend == backend && g_model_path == model_path.u8string() &&
            g_model_gpu_layers == config.n_gpu_layers && g_model_size == model_size && g_model_modified == model_modified;
        if (!reusable) {
            backend->unload_model();
            g_model_backend = nullptr;
            const auto load_start = Clock::now();
            const char* reason = "error";
            std::string message;
            if (!HashModelFile(model_path, trace, &g_model_digest, &reason, &message)) return fail(reason, message);
            if (!config.model_sha256.empty() && config.model_sha256 != g_model_digest)
                return fail("model_corrupt", "Model SHA-256 differs from the packaged manifest");
            g_model_info = LlmBackendModel();
            const bool loaded = backend->load_model(config, trace.liveness(), &g_model_info, &message);
            result->load_time_ms = ElapsedMs(load_start);
            if (trace.cancelled || trace.expired()) {
                backend->unload_model();
                return fail(trace.cancelled ? "cancelled" : "timeout", "Stopped while loading model");
            }
            if (!loaded) return fail("model_load_error", message);
            g_model_backend = backend;
            g_model_path = model_path.u8string();
            g_model_gpu_layers = config.n_gpu_layers;
            g_model_size = model_size;
            g_model_modified = model_modified;
        }
        if (!config.model_sha256.empty() && config.model_sha256 != g_model_digest)
            return fail("model_corrupt", "Resident model differs from the selected manifest");
        config.model_sha256 = g_model_digest;
        result->model_reused = reusable;
        result->offloaded_layers = g_model_info.offloaded_layers;
        result->total_layers = g_model_info.total_layers;
        std::string error;
        SessionCloser session{backend, backend->open(messages, &result->prompt_text, &result->prompt_tokens, &error)};
        if (!session.session) return fail("error", error);
        if(count_only) {result->success=true;return true;}
        const int reserve = config.n_predict;
        if (result->prompt_tokens + reserve + 1 > config.n_ctx) return fail("context_limit", "Prompt and reserved output exceed fixed context budget");
        if (!backend->start(session.session, config, trace.liveness(), &error)) return fail("error", error);
        trace.inference_started = Clock::now();
        trace.inferring = true;
        struct InferenceEnd {
            GenerationTrace& trace;
            ~InferenceEnd() {
                trace.result->inference_time_ms = ElapsedMs(trace.inference_started);
                trace.inferring = false;
            }
        } inference_end{trace};
        for (int remaining = result->prompt_tokens; remaining > 0;) {
            if (trace.expired()) return fail("timeout", "Prompt deadline exceeded");
            if (stream_callback && !stream_callback("", "", stream_user_data)) return fail("cancelled", "Cancelled before generation");
            if (!backend->prefill(session.session, config.n_batch, &remaining, &error))
                return fail(trace.cancelled ? "cancelled" : trace.expired() ? "timeout" : "error", error);
        }
        result->prompt_time_ms = ElapsedMs(trace.inference_started);
        const auto generation_started = Clock::now();
        std::string streamed_final;
        for (int generated = 0; generated <= reserve; ++generated) {
            if (trace.expired()) return fail("timeout", "Generation deadline exceeded");
            if (stream_callback && !stream_callback(streamed_final.c_str(), "", stream_user_data)) return fail("cancelled", "Generation cancelled");
            int32_t token = 0;
            bool eog = false;
            std::string piece;
            if (!backend->sample(session.session, &token, &eog, &piece, &error)) { result->error_message = error; return false; }
            if (eog) { result->reached_eog = true; break; }
            if (generated == reserve) return fail("token_limit", "Total generation budget exhausted");
            result->raw_text += piece;
            ++result->generated_tokens;
            const auto raw_guard = GuardFinalChannel(result->raw_text, false);
            if (!raw_guard.error.empty()) return fail("protocol_error", raw_guard.error);
            ++result->final_tokens;
            std::string content;
            if (!backend->feed(session.session, piece, token, &content, &error)) return fail("protocol_error", error);
            const auto channels = GuardFinalChannel(content, false);
            if (!channels.error.empty()) return fail("protocol_error", channels.error);
            if (channels.text.compare(0, streamed_final.size(), streamed_final) != 0)
                return fail("protocol_error", "Final stream changed its prefix");
            const std::string delta = channels.text.substr(streamed_final.size());
            streamed_final = channels.text;
            if (stream_callback && !delta.empty() && !stream_callback(streamed_final.c_str(), delta.c_str(), stream_user_data))
                return fail("cancelled", "Generation cancelled");
            if (!backend->decode(session.session, token, &error))
                return fail(trace.cancelled ? "cancelled" : trace.expired() ? "timeout" : "error", error);
        }
        result->generation_time_ms = ElapsedMs(generation_started);
        if (!result->reached_eog) return fail("token_limit", "Missing end of generation");
        const auto raw_guard = GuardFinalChannel(result->raw_text, true);
        if (!raw_guard.error.empty()) return fail("protocol_error", raw_guard.error);
        std::string content;
        if (!backend->finish(session.session, &content, &error)) return fail("protocol_error", error);
        const auto channels = GuardFinalChannel(content, true);
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

    fprintf(out, "llama.cpp compiled in: %s\n", info.compiled_with_llama ? "yes" : "no");
    fprintf(out, "llama.cpp vendored commit: %s\n", info.vendored_commit ? info.vendored_commit : "unknown");
    fprintf(out, "GPU offload available: %s\n", info.gpu_offload_supported ? "yes" : "no");

    if (!info.system_info.empty()) {
        fprintf(out, "System info: %s\n", info.system_info.c_str());
    }
}

}  // namespace liminal
