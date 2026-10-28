#pragma once
// Public, step-wise API of the dedicated Gemma 4 E4B engine. No MLX or Vulkan
// types leak here; the game's runtime calls one step at a time so it can poll
// cancellation and deadlines between steps.
#include <cstdint>
#include <string>
#include <vector>

namespace liminal::engine {

struct Engine;
struct EngineSession;

enum class EngineDevice {
    kDefault,       // best compiled device for this platform
    kCpuReference,  // scalar f32 oracle; slow, for tests and parity
    kMlx,           // MLX/Metal (macOS)
    kVulkan,        // native Vulkan (Windows, Linux; MoltenVK for development)
};

struct EngineMessage {
    std::string role;
    std::string content;
};

struct EngineSampling {
    float temperature = 0.0f;
    int top_k = 40;
    float top_p = 0.95f;
    uint32_t seed = 42;
    bool json_constraint = false;
    std::string json_schema;  // empty with json_constraint: any JSON object
};

bool EngineLoad(const std::string& gguf_path, EngineDevice device, Engine** engine, std::string* error);
void EngineFree(Engine* engine);
const char* EngineDeviceName(const Engine* engine);
int EngineLayerCount(const Engine* engine);

bool EngineRenderChat(Engine* engine, const std::vector<EngineMessage>& messages, std::string* prompt_text,
    std::vector<int32_t>* tokens);
const std::string& EngineTokenPiece(const Engine* engine, int32_t token);

EngineSession* EngineSessionBegin(Engine* engine, const std::vector<int32_t>& prompt, const EngineSampling& sampling,
    int context_tokens, std::string* error);
// Evaluates up to max_tokens prompt tokens; *remaining is 0 once the prompt is consumed.
bool EngineSessionPrefill(EngineSession* session, int max_tokens, int* remaining, std::string* error);
// Picks the next token from the current logits (constraint applied and advanced).
bool EngineSessionSample(EngineSession* session, int32_t* token, bool* eog, std::string* error);
// Evaluates a sampled token so the next sample sees it.
bool EngineSessionDecode(EngineSession* session, int32_t token, std::string* error);
void EngineSessionEnd(EngineSession* session);

}  // namespace liminal::engine
