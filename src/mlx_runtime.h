#pragma once
#include "llm_runtime.h"
namespace liminal {
// Caller holds the common runtime mutex. No sockets are exposed to the network.
bool GenerateMlxCompletion(const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&,
    LlmStreamCallback, void*, LlmGenerationResult*, bool count_only);
void ShutdownMlxRuntime();
}
