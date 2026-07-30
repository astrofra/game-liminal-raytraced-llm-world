#include "llm_runtime.h"

#include <string.h>

#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "llama.h"
#endif

namespace liminal {

namespace {

#if !defined(LIMINAL_LLAMA_VENDORED_COMMIT)
#define LIMINAL_LLAMA_VENDORED_COMMIT "unknown"
#endif

static bool g_llama_backend_initialized = false;

}  // namespace

bool InitializeLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!g_llama_backend_initialized) {
        llama_backend_init();
        g_llama_backend_initialized = true;
    }
    return true;
#else
    return false;
#endif
}

void ShutdownLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (g_llama_backend_initialized) {
        llama_backend_free();
        g_llama_backend_initialized = false;
    }
#endif
}

bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info)
{
    if (!info) {
        return false;
    }

    info->compiled_with_llama = false;
    info->gpu_offload_supported = false;
    info->vendored_commit = LIMINAL_LLAMA_VENDORED_COMMIT;
    info->system_info.clear();

#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!InitializeLlmRuntime()) {
        return false;
    }

    info->compiled_with_llama = true;
    info->gpu_offload_supported = llama_supports_gpu_offload();

    const char* system_info = llama_print_system_info();
    if (system_info && system_info[0]) {
        info->system_info = system_info;
    }

    return true;
#else
    info->system_info = "llama.cpp support is not compiled into this binary.";
    return false;
#endif
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
