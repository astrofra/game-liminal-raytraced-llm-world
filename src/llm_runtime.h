#ifndef LIMINAL_RENDERER_LLM_RUNTIME_H
#define LIMINAL_RENDERER_LLM_RUNTIME_H

#include <stdio.h>
#include <string>
#include <vector>

namespace liminal {

struct LlmPromptMessage {
    std::string role;
    std::string content;
};

struct LlmGenerationConfig {
    std::string model_path;
    std::string grammar_path;
    int n_ctx;
    int n_batch;
    int n_ubatch;
    int reasoning_budget;
    int timeout_ms;
    int top_k;
    float top_p;
    bool reasoning;
    bool use_jinja;
    bool short_call;
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
    double load_time_ms = 0.0;
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
bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmGenerationResult* result);
void PrintLlmRuntimeInfo(FILE* stream);

}  // namespace liminal

#endif
