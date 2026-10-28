#pragma once
#include "llm_runtime.h"
namespace liminal {
bool GenerateMlxCppCompletion(const LlmGenerationConfig &, const std::vector<LlmPromptMessage> &,
                              LlmStreamCallback, void *, LlmGenerationResult *, bool count_only);
void ShutdownMlxCppRuntime();
} // namespace liminal
