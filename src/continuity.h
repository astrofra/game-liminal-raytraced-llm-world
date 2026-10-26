#ifndef LIMINAL_CONTINUITY_H
#define LIMINAL_CONTINUITY_H

#include "llm_runtime.h"
#include <functional>

namespace liminal {
// Structured data, never instructions. Only published engine events enter history.
struct ContinuityContextV1 {
    std::string core_json = "{}";
    std::vector<std::string> events_json;
    std::vector<std::string> notes;
};
using ContinuityPromptBuilder = std::function<std::vector<LlmPromptMessage>(const ContinuityContextV1&)>;
using ContinuityTokenCounter = std::function<bool(const std::vector<LlmPromptMessage>&, int*, std::string*)>;
std::string SerializeContinuityContext(const ContinuityContextV1& context);
// Counts whole templated requests, including repair candidates. Does not mutate memory.
bool FitContinuityPrompt(const ContinuityContextV1& context, int context_tokens, int output_tokens,
                         int memory_tokens, const ContinuityPromptBuilder& build,
                         const ContinuityTokenCounter& count, std::vector<LlmPromptMessage>* messages,
                         std::string* metrics, std::string* error);
}
#endif
