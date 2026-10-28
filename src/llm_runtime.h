#ifndef LIMINAL_RENDERER_LLM_RUNTIME_H
#define LIMINAL_RENDERER_LLM_RUNTIME_H

#include <stdio.h>
#include <string>
#include <vector>
#include <functional>

namespace liminal {

struct LlmPromptMessage {
    std::string role;
    std::string content;
};

struct LlmGenerationConfig {
    int continuity_memory_tokens = 1024;
    // Deterministic tokenizer seam for tests. Production uses the resident model/template.
    std::function<bool(const std::vector<LlmPromptMessage>&, int*, std::string*)> prompt_token_counter;
    std::string model_path;
    std::string model_sha256;
    std::string json_schema; // Optional schema for constrained final JSON; no effect on ordinary calls.
    int n_ctx;
    int n_batch;  // prompt tokens evaluated per prefill step (cancellation granularity)
    int timeout_ms;
    int top_k;
    float top_p;
    std::string profile_name;
    std::string purpose;
    int n_predict;
    unsigned int seed;
    float temperature;
    bool use_json_grammar;

    LlmGenerationConfig();

};

struct LlmGenerationResult {
    bool success;
    std::string prompt_text;
    std::string response_text; // Final channel only.
    std::string raw_text;
    std::string reasoning_text;
    std::string stop_reason;
    int reasoning_tokens = 0;
    int final_tokens = 0;
    int total_layers = -1;
    bool model_reused = false;
    double load_time_ms = 0.0;
    double prompt_time_ms = 0.0;
    double generation_time_ms = 0.0;
    double wall_time_ms = 0.0;
    std::string error_message;
    int prompt_tokens;
    int generated_tokens;
    bool reached_eog;
    double inference_time_ms;

    LlmGenerationResult()
        : success(false)
        , prompt_tokens(0)
        , generated_tokens(0)
        , reached_eog(false)
        , inference_time_ms(0.0)
    {
    }
};

struct LlmRuntimeInfo {
    std::string backend;        // "native"
    std::string engine_commit;  // source revision of the engine build
    std::string device;         // device of the resident model, or the requested one
    std::string system_info;
};

struct LlmRuntimeTotals {
    int calls = 0;
    int prompt_tokens = 0;
    int generated_tokens = 0;
    int reasoning_tokens = 0;
    int final_tokens = 0;
    double inference_time_ms = 0.0;
    double wall_time_ms = 0.0;
};

LlmRuntimeTotals QueryLlmRuntimeTotals();

// Return false to cancel. Empty deltas are liveness polls during prompt/decode;
// consumers must not interpret an empty delta as an error or completed response.
typedef bool (*LlmStreamCallback)(const char* accumulated_text, const char* delta_text, void* user_data);

bool ConfigureLlmProfile(const std::string& name, LlmGenerationConfig* config, std::string* error);
// Overrides LIMINAL_LLM_BACKEND: native (best device), mlx, vulkan or cpu-ref; unloads a
// model resident with another choice.
bool SelectLlmBackend(const std::string& name, std::string* error);
bool InitializeLlmRuntime();
void ShutdownLlmRuntime();
bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info);
// "backend:device" of the resident model (backend name alone before loading), for archives.
std::string LlmRuntimeLabel();
bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmStreamCallback stream_callback,
    void* stream_user_data,
    LlmGenerationResult* result);
// Loads/verifies the same model if needed; templates/tokenizes without decoding or an LLM call.
bool CountChatPromptTokens(const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages, LlmStreamCallback callback, void* user,
    int* tokens, std::string* error);
bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmGenerationResult* result);
void PrintLlmRuntimeInfo(FILE* stream);

}  // namespace liminal

#endif
