#ifndef LIMINAL_RENDERER_LLM_RUNTIME_H
#define LIMINAL_RENDERER_LLM_RUNTIME_H

#include <stdio.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>

namespace liminal {

struct LlmPromptMessage {
    std::string role;
    std::string content;
};

// Shared by all calls and repairs in one cloud interaction; never persisted as world state.
struct LlmCloudBudget {
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    int remaining_calls = 24;
    int remaining_output_tokens = 32768;
    bool failed = false;
};

struct LlmGenerationConfig {
    std::string backend = "local";
    std::string cloud_model = "deepseek-v4.1-flash";
    std::shared_ptr<LlmCloudBudget> cloud_budget;
    // Cosmetic rewrite only: invalid cloud content may keep the validated prose.
    // Transport, cancellation, deadlines and shared budgets remain fatal.
    bool cloud_optional_prose = false;
    int continuity_memory_tokens = 1024;
    // Deterministic tokenizer seam for tests. Production uses the resident model/template.
    std::function<bool(const std::vector<LlmPromptMessage>&, int*, std::string*)> prompt_token_counter;
    // Receives request/response JSON and the exact raw bytes of a response.
    std::function<void(bool, const std::string&, const std::string&)> research_trace;
    std::string model_path;
    std::string model_sha256;
    std::string json_schema; // Optional schema for constrained final JSON; no effect on ordinary calls.
    int n_ctx;
    int n_batch;
    int n_ubatch;
    int timeout_ms;
    int top_k;
    float top_p;
    std::string profile_name;
    std::string purpose;
    int n_threads;
    int n_threads_batch;
    int n_predict;
    int n_gpu_layers;
    unsigned int seed;
    float temperature;
    bool flash_attention;
    bool use_json_grammar;

    LlmGenerationConfig();

};

struct LlmGenerationResult {
    std::string reported_model;
    bool usage_reported = false;
    int cloud_attempts = 0;
    bool success;
    std::string prompt_text;
    std::string response_text; // Final channel only.
    std::string raw_text;
    std::string reasoning_text;
    std::string stop_reason;
    int reasoning_tokens = 0;
    int final_tokens = 0;
    int offloaded_layers = -1;
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
    bool compiled_with_llama;
    bool gpu_offload_supported;
    const char* vendored_commit;
    std::string system_info;

    LlmRuntimeInfo()
        : compiled_with_llama(false)
        , gpu_offload_supported(false)
        , vendored_commit("not-vendored")
    {
    }
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
bool InitializeLlmRuntime();
void ShutdownLlmRuntime();
bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info);
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
