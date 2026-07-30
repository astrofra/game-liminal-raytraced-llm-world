#ifndef LIMINAL_RENDERER_LLM_RUNTIME_H
#define LIMINAL_RENDERER_LLM_RUNTIME_H

#include <stdio.h>
#include <string>

namespace liminal {

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

bool InitializeLlmRuntime();
void ShutdownLlmRuntime();
bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info);
void PrintLlmRuntimeInfo(FILE* stream);

}  // namespace liminal

#endif
