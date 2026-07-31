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
    int n_threads;
    int n_threads_batch;
    int n_predict;
    int n_gpu_layers;
    unsigned int seed;
    float temperature;
    bool flash_attention;
    bool use_json_grammar;

    LlmGenerationConfig()
        : model_path("models/ministral-3-8b/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf")
        , grammar_path("vendor/llama.cpp/grammars/json.gbnf")
        , n_ctx(4096)
        , n_batch(2048)
        , n_threads(0)
        , n_threads_batch(0)
        , n_predict(768)
        , n_gpu_layers(-1)
        , seed(42u)
        , temperature(0.0f)
        , flash_attention(true)
        , use_json_grammar(false)
    {
    }
};

struct LlmGenerationResult {
    bool success;
    std::string prompt_text;
    std::string response_text;
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

typedef bool (*LlmStreamCallback)(const char* accumulated_text, const char* delta_text, void* user_data);

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
