#pragma once
#include "llm_runtime.h"
#include "nlohmann/json.hpp"

namespace liminal {
// Byte chunks may split a JSON record or a UTF-8 code point. No HTTP headers are exposed.
using CloudChunk = std::function<bool(const std::string&)>;
using CloudAlive = std::function<bool()>;
using CloudTransport = std::function<bool(const std::string&, const CloudChunk&, const CloudAlive&, std::string*)>;
bool ValidateCloudJson(const nlohmann::json& value, const nlohmann::json& schema, std::string* error);
int EstimateCloudPromptTokens(const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&);
bool GenerateOllamaCompletion(const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&,
    LlmStreamCallback, void*, LlmGenerationResult*, const CloudTransport& transport = {});
bool CheckOllamaConfiguration(const LlmGenerationConfig&, std::string* error);
}
