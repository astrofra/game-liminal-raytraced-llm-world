#include "llm_backend.h"

#if defined(LIMINAL_HAVE_NATIVE_LLM)
#include "liminal/llm_engine.h"

#include <memory>

#if !defined(LIMINAL_ENGINE_COMMIT)
#define LIMINAL_ENGINE_COMMIT "dev"
#endif

namespace liminal {

namespace {

namespace eng = liminal::engine;

static eng::Engine* g_engine = nullptr;
static eng::EngineDevice g_requested_device = eng::EngineDevice::kDefault;

struct NativeSession {
    std::vector<int32_t> prompt;
    eng::EngineSession* session = nullptr;
    std::string content;
    ~NativeSession() { eng::EngineSessionEnd(session); }
};

static NativeSession* Cast(LlmBackendSession* session) { return reinterpret_cast<NativeSession*>(session); }

static bool Initialize(std::string*) { return true; }

static void UnloadModel()
{
    eng::EngineFree(g_engine);
    g_engine = nullptr;
}

static void Shutdown() { UnloadModel(); }

static void Describe(LlmRuntimeInfo* info)
{
    info->engine_commit = LIMINAL_ENGINE_COMMIT;
    info->device = g_engine ? eng::EngineDeviceName(g_engine) : "not loaded";
    info->system_info = "Dedicated Gemma 4 E4B engine";
}

static bool LoadModel(const LlmGenerationConfig& config, LlmLiveness live, LlmBackendModel* model, std::string* error)
{
    UnloadModel();
    if (!live.Check()) { *error = "Stopped before loading model"; return false; }
    if (!eng::EngineLoad(config.model_path, g_requested_device, &g_engine, error)) {
        *error = "Failed to load " + config.model_path + ": " + *error;
        return false;
    }
    model->device = eng::EngineDeviceName(g_engine);
    model->total_layers = eng::EngineLayerCount(g_engine);
    return true;
}

static LlmBackendSession* Open(const std::vector<LlmPromptMessage>& messages, std::string* prompt_text,
    int* prompt_tokens, std::string* error)
{
    if (!g_engine) { *error = "No resident model"; return nullptr; }
    std::unique_ptr<NativeSession> session(new NativeSession());
    std::vector<eng::EngineMessage> chat;
    chat.reserve(messages.size());
    for (const auto& message : messages) chat.push_back({message.role, message.content});
    if (!eng::EngineRenderChat(g_engine, chat, prompt_text, &session->prompt)) { *error = "Prompt rendering failed"; return nullptr; }
    *prompt_tokens = static_cast<int>(session->prompt.size());
    return reinterpret_cast<LlmBackendSession*>(session.release());
}

static bool Start(LlmBackendSession* handle, const LlmGenerationConfig& config, LlmLiveness, std::string* error)
{
    eng::EngineSampling sampling;
    sampling.temperature = config.temperature;
    sampling.top_k = config.top_k;
    sampling.top_p = config.top_p;
    sampling.seed = config.seed;
    sampling.json_constraint = config.use_json_grammar;
    sampling.json_schema = config.json_schema;
    NativeSession* session = Cast(handle);
    session->session = eng::EngineSessionBegin(g_engine, session->prompt, sampling, config.n_ctx, error);
    return session->session != nullptr;
}

static bool Prefill(LlmBackendSession* handle, int max_tokens, int* remaining, std::string* error)
{
    return eng::EngineSessionPrefill(Cast(handle)->session, max_tokens, remaining, error);
}

static bool Sample(LlmBackendSession* handle, int32_t* token, bool* eog, std::string* piece, std::string* error)
{
    if (!eng::EngineSessionSample(Cast(handle)->session, token, eog, error)) return false;
    piece->clear();
    if (!*eog) *piece = eng::EngineTokenPiece(g_engine, *token);
    return true;
}

// Final-only generation: the visible answer is the raw text; the runtime's
// channel guard rejects any control marker.
static bool Feed(LlmBackendSession* handle, const std::string& piece, int32_t, std::string* content, std::string*)
{
    NativeSession* session = Cast(handle);
    session->content += piece;
    *content = session->content;
    return true;
}

static bool Decode(LlmBackendSession* handle, int32_t token, std::string* error)
{
    return eng::EngineSessionDecode(Cast(handle)->session, token, error);
}

static bool Finish(LlmBackendSession* handle, std::string* content, std::string*)
{
    *content = Cast(handle)->content;
    return true;
}

static void Close(LlmBackendSession* handle) { delete Cast(handle); }

static const LlmBackend kNativeBackend = {
    "native", LIMINAL_ENGINE_COMMIT, Initialize, Shutdown, Describe, LoadModel, UnloadModel,
    Open, Start, Prefill, Sample, Feed, Decode, Finish, Close,
};

}  // namespace

const LlmBackend* NativeLlmBackend(const std::string& device)
{
    if (device == "cpu-ref") g_requested_device = eng::EngineDevice::kCpuReference;
    else if (device == "mlx") g_requested_device = eng::EngineDevice::kMlx;
    else if (device == "vulkan") g_requested_device = eng::EngineDevice::kVulkan;
    else g_requested_device = eng::EngineDevice::kDefault;
    return &kNativeBackend;
}

}  // namespace liminal

#else

namespace liminal {
const LlmBackend* NativeLlmBackend(const std::string&) { return nullptr; }
}

#endif
