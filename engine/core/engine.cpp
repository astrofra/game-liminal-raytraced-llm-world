#include "liminal/llm_engine.h"

#include "core/chat_format.h"
#include "core/e4b_spec.h"
#include "core/gguf.h"
#include "core/sampler.h"
#include "core/tokenizer.h"
#include "ref/forward_ref.h"
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
#include "core/grammar.h"
#endif
#if defined(LIMINAL_ENGINE_HAVE_MLX)
#include "mlx/mlx_backend.h"
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
#include "vulkan/vulkan_backend.h"
#endif

#include <algorithm>
#include <cstdio>
#include <future>
#include <memory>

namespace liminal::engine {

struct Engine {
    EngineDevice device = EngineDevice::kCpuReference;
    GgufFile file;
    E4bModel model;
    Tokenizer tokenizer;
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    GrammarContext* grammar = nullptr;
#endif
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    MlxModel* mlx = nullptr;
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    VulkanModel* vulkan = nullptr;
#endif
};

struct EngineSession {
    Engine* engine = nullptr;
    std::vector<int32_t> prompt;
    size_t consumed = 0;
    int context_tokens = 0;
    Sampler sampler;
    RefState state;
    std::vector<float> logits;
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    MlxState* mlx = nullptr;
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    VulkanState* vulkan = nullptr;
#endif
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    // The schema compiles on a worker while the prompt is evaluated; the first
    // sample waits for it. Declared after the error string the worker writes,
    // so destruction waits for the worker first.
    std::string constraint_error;
    std::future<JsonConstraint*> pending_constraint;
    JsonConstraint* constraint = nullptr;
#endif
};

bool EngineLoad(const std::string& gguf_path, EngineDevice device, Engine** out, std::string* error)
{
    std::unique_ptr<Engine> engine(new Engine());
    // Default: MLX on Apple, else Vulkan, else the CPU reference.
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    const EngineDevice preferred = EngineDevice::kMlx;
#elif defined(LIMINAL_ENGINE_HAVE_VULKAN)
    const EngineDevice preferred = EngineDevice::kVulkan;
#else
    const EngineDevice preferred = EngineDevice::kCpuReference;
#endif
    engine->device = device == EngineDevice::kDefault ? preferred : device;
    if (!GgufOpen(gguf_path, &engine->file, error)) return false;
    if (!E4bBind(engine->file, &engine->model, error)) return false;
    if (!TokenizerLoad(engine->file, &engine->tokenizer, error)) return false;
    if (engine->device == EngineDevice::kMlx) {
#if defined(LIMINAL_ENGINE_HAVE_MLX)
        engine->mlx = MlxLoad(engine->model, error);
        if (!engine->mlx) return false;
#else
        *error = "MLX is not available in this engine build";
        return false;
#endif
    }
    if (engine->device == EngineDevice::kVulkan) {
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
        engine->vulkan = VulkanLoad(engine->model, error);
        if (!engine->vulkan) {
            if (device != EngineDevice::kDefault) return false;
            // No usable GPU: the slow CPU reference keeps the game playable.
            fprintf(stderr, "[llm] %s; falling back to the CPU reference\n", error->c_str());
            error->clear();
            engine->device = EngineDevice::kCpuReference;
        }
#else
        *error = "Vulkan is not available in this engine build";
        return false;
#endif
    }
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    engine->grammar = GrammarContextCreate(engine->tokenizer, error);
    if (!engine->grammar) return false;
#endif
    *out = engine.release();
    return true;
}

void EngineFree(Engine* engine)
{
    if (!engine) return;
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    GrammarContextFree(engine->grammar);
#endif
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    MlxFree(engine->mlx);
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    VulkanFree(engine->vulkan);
#endif
    GgufClose(&engine->file);
    delete engine;
}

const char* EngineDeviceName(const Engine* engine)
{
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    if (engine->device == EngineDevice::kMlx) return MlxDeviceName(engine->mlx);
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    if (engine->device == EngineDevice::kVulkan) return VulkanDeviceName(engine->vulkan);
#endif
    return engine->device == EngineDevice::kCpuReference ? "cpu-reference" : "unknown";
}

int EngineLayerCount(const Engine*) { return kE4bLayers; }

bool EngineRenderChat(Engine* engine, const std::vector<EngineMessage>& messages, std::string* prompt_text,
    std::vector<int32_t>* tokens)
{
    std::vector<ChatMessage> chat;
    chat.reserve(messages.size());
    for (const auto& message : messages) chat.push_back({message.role, message.content});
    ChatPrompt prompt = RenderChat(engine->tokenizer, chat);
    *prompt_text = std::move(prompt.text);
    *tokens = std::move(prompt.tokens);
    return true;
}

const std::string& EngineTokenPiece(const Engine* engine, int32_t token)
{
    return TokenPiece(engine->tokenizer, token);
}

EngineSession* EngineSessionBegin(Engine* engine, const std::vector<int32_t>& prompt, const EngineSampling& sampling,
    int context_tokens, std::string* error)
{
    if (prompt.empty()) { *error = "Empty prompt"; return nullptr; }
    std::unique_ptr<EngineSession> session(new EngineSession());
    session->engine = engine;
    session->prompt = prompt;
    session->context_tokens = context_tokens;
    SamplerInit(&session->sampler, {sampling.temperature, sampling.top_k, sampling.top_p, sampling.seed});
    if (sampling.json_constraint) {
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
        EngineSession* raw = session.get();
        raw->pending_constraint = std::async(std::launch::async, [grammar = engine->grammar, schema = sampling.json_schema, raw] {
            return JsonConstraintBegin(grammar, schema, &raw->constraint_error);
        });
#else
        *error = "JSON constraint is not available in this engine build";
        return nullptr;
#endif
    }
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    if (engine->mlx) session->mlx = MlxStateCreate(engine->mlx, context_tokens);
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    if (engine->vulkan) {
        session->vulkan = VulkanStateCreate(engine->vulkan, context_tokens, error);
        if (!session->vulkan) return nullptr;
    }
#endif
    return session.release();
}

// Evaluates tokens on the session's device and refreshes the last logits.
static bool Forward(EngineSession* session, const int32_t* tokens, int count, std::string* error)
{
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    if (session->mlx) return MlxForward(session->engine->mlx, session->mlx, tokens, count, &session->logits, error);
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    if (session->vulkan) return VulkanForward(session->engine->vulkan, session->vulkan, tokens, count, &session->logits, error);
#endif
    if (session->state.n_past + count > session->context_tokens) { *error = "Context exhausted"; return false; }
    RefForward(session->engine->model, &session->state, tokens, count, &session->logits);
    return true;
}

bool EngineSessionPrefill(EngineSession* session, int max_tokens, int* remaining, std::string* error)
{
    const size_t left = session->prompt.size() - session->consumed;
    const int count = static_cast<int>(std::min<size_t>(static_cast<size_t>(std::max(1, max_tokens)), left));
    if (count > 0) {
        if (!Forward(session, session->prompt.data() + session->consumed, count, error)) return false;
        session->consumed += static_cast<size_t>(count);
    }
    *remaining = static_cast<int>(session->prompt.size() - session->consumed);
    return true;
}

bool EngineSessionSample(EngineSession* session, int32_t* token, bool* eog, std::string* error)
{
    if (session->logits.empty()) { *error = "Sampling before the prompt was evaluated"; return false; }
    const uint32_t* allowed = nullptr;
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    if (session->pending_constraint.valid()) {
        session->constraint = session->pending_constraint.get();
        if (!session->constraint) { *error = session->constraint_error; return false; }
    }
    if (session->constraint) allowed = JsonConstraintMask(session->constraint);
#endif
    const int32_t picked = SamplerPick(&session->sampler, session->logits.data(), static_cast<int>(session->logits.size()), allowed);
    if (picked < 0) { *error = "No token satisfies the JSON constraint"; return false; }
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    if (session->constraint && !JsonConstraintAccept(session->constraint, picked)) { *error = "Constraint rejected the sampled token"; return false; }
#endif
    *token = picked;
    *eog = IsEog(session->engine->tokenizer, picked);
    return true;
}

bool EngineSessionDecode(EngineSession* session, int32_t token, std::string* error)
{
    return Forward(session, &token, 1, error);
}

void EngineSessionEnd(EngineSession* session)
{
    if (!session) return;
#if defined(LIMINAL_ENGINE_HAVE_XGRAMMAR)
    if (session->pending_constraint.valid()) session->constraint = session->pending_constraint.get();
    JsonConstraintFree(session->constraint);
#endif
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    MlxStateFree(session->mlx);
#endif
#if defined(LIMINAL_ENGINE_HAVE_VULKAN)
    VulkanStateFree(session->vulkan);
#endif
    delete session;
}

}  // namespace liminal::engine
