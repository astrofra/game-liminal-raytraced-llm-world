#ifndef LIMINAL_RENDERER_LLM_BACKEND_H
#define LIMINAL_RENDERER_LLM_BACKEND_H

#include "llm_runtime.h"
#include <cstdint>
#include <string>
#include <vector>

namespace liminal {

// Internal seam between the public runtime contract (llm_runtime.cpp) and one
// inference backend. The runtime owns locking, model verification, deadlines,
// streaming, channel guards and metrics; a backend only loads, tokenizes and
// steps. Every step returns to the runtime so cancellation stays per chunk/token.

// Returns false once the request is cancelled or past its deadline.
struct LlmLiveness {
    bool (*alive)(void* data) = nullptr;
    void* data = nullptr;
    bool Check() const { return !alive || alive(data); }
};

struct LlmBackendModel {
    int offloaded_layers = -1;
    int total_layers = -1;
    std::string device;
};

struct LlmBackendSession;

struct LlmBackend {
    const char* name;    // reported as "backend" in LIMINAL_LLM_METRIC
    const char* commit;  // reported as "runtime_commit"
    bool (*initialize)(std::string* error);
    void (*shutdown)();
    void (*describe)(LlmRuntimeInfo* info);
    // The file was already verified against its SHA-256 by the runtime.
    bool (*load_model)(const LlmGenerationConfig& config, LlmLiveness live, LlmBackendModel* model, std::string* error);
    void (*unload_model)();
    // Renders the chat template and tokenizes; no device work.
    LlmBackendSession* (*open)(const std::vector<LlmPromptMessage>& messages, std::string* prompt_text,
        int* prompt_tokens, std::string* error);
    // Allocates the decoding state and sampler (JSON constraint included).
    bool (*start)(LlmBackendSession* session, const LlmGenerationConfig& config, LlmLiveness live, std::string* error);
    // Evaluates up to max_tokens more prompt tokens; *remaining reaches zero when the prompt is consumed.
    bool (*prefill)(LlmBackendSession* session, int max_tokens, int* remaining, std::string* error);
    // Samples and accepts the next token. On EOG, *eog is set and nothing else is produced.
    bool (*sample)(LlmBackendSession* session, int32_t* token, bool* eog, std::string* piece, std::string* error);
    // Parses the visible answer after appending piece; *content is the whole final channel so far.
    bool (*feed)(LlmBackendSession* session, const std::string& piece, int32_t token, std::string* content, std::string* error);
    // Evaluates the accepted token so the next sample sees it.
    bool (*decode)(LlmBackendSession* session, int32_t token, std::string* error);
    bool (*finish)(LlmBackendSession* session, std::string* content, std::string* error);
    void (*close)(LlmBackendSession* session);
};

const LlmBackend* LlamaLlmBackend();  // nullptr when llama.cpp is not compiled in
// Dedicated E4B engine; device "cpu-ref" forces the scalar oracle. nullptr when not compiled in.
const LlmBackend* NativeLlmBackend(const std::string& device);

}  // namespace liminal

#endif
