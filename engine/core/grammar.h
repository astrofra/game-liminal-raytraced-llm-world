#pragma once
// JSON-schema constrained decoding with xgrammar. Schemas are normalized to
// llama.cpp's json-schema-to-grammar semantics before compilation:
// an object with "properties" and no "additionalProperties" admits no extra keys,
// a bare {"type":"object"} admits any object. Control tokens other than the
// end-of-generation set are never allowed inside a constrained answer.
#include "core/tokenizer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace liminal::engine {

struct GrammarContext;     // per model: tokenizer info + compiled-schema cache
struct JsonConstraint;     // per request: matcher state

GrammarContext* GrammarContextCreate(const Tokenizer& tokenizer, std::string* error);
void GrammarContextFree(GrammarContext* context);

JsonConstraint* JsonConstraintBegin(GrammarContext* context, const std::string& schema, std::string* error);
// One bit per token, valid until the next call on this constraint.
const uint32_t* JsonConstraintMask(JsonConstraint* constraint);
bool JsonConstraintAccept(JsonConstraint* constraint, int32_t token);
void JsonConstraintFree(JsonConstraint* constraint);

}  // namespace liminal::engine
