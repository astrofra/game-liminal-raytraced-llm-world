#ifndef LIMINAL_OLLAMA_RUNTIME_H
#define LIMINAL_OLLAMA_RUNTIME_H
#include "llm_runtime.h"
namespace liminal {
// Fixed loopback endpoint; independent messages per call, resident weights in Ollama.
bool GenerateOllamaCompletion(const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&,
                             LlmStreamCallback, void*, LlmGenerationResult*);
}
#endif
