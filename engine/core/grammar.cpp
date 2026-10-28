#include "core/grammar.h"

#include "nlohmann/json.hpp"
#include <xgrammar/xgrammar.h>
// Internal header of the pinned xgrammar: the schema-to-EBNF converter.
#include "json_schema_converter.h"

#include <mutex>
#include <stdexcept>
#include <thread>

namespace liminal::engine {

namespace {

// Property order matters to the grammar: keep the schema's key order.
using json = nlohmann::ordered_json;

// llama.cpp common/json-schema.cpp: properties without additionalProperties
// forbid extra keys; an object schema without properties accepts any object.
static void NormalizeSchema(json* node)
{
    if (node->is_array()) {
        for (auto& item : *node) NormalizeSchema(&item);
        return;
    }
    if (!node->is_object()) return;
    for (auto& [key, value] : node->items()) {
        if (key != "const" && key != "enum" && key != "default" && key != "examples") NormalizeSchema(&value);
    }
    const auto type = node->find("type");
    const bool object = type != node->end() && ((type->is_string() && *type == "object") ||
        (type->is_array() && std::find(type->begin(), type->end(), "object") != type->end()));
    if ((object || node->contains("properties")) && !node->contains("additionalProperties"))
        (*node)["additionalProperties"] = !node->contains("properties");
}

// xgrammar spells every whitespace run of an any-whitespace schema as
// [ \n\t]{0,N}. llama.cpp's json-schema-to-grammar uses
//   space ::= | " " | "\n"{1,2} [ \t]{0,20}
// so a sentinel count marks those runs and each one is replaced by that rule.
static std::string SchemaToEbnf(const std::string& schema)
{
    constexpr int kSentinel = 977;
    std::string ebnf = xgrammar::JSONSchemaToEBNF(schema, true, std::nullopt, std::nullopt, false, kSentinel);
    const std::string run = "[ \\n\\t]{0," + std::to_string(kSentinel) + "}";
    const std::string space = "(\"\" | \" \" | \"\\n\" [ \\t]{0,20} | \"\\n\\n\" [ \\t]{0,20})";
    size_t replaced = 0;
    for (size_t at = ebnf.find(run); at != std::string::npos; at = ebnf.find(run, at + space.size())) {
        ebnf.replace(at, run.size(), space);
        ++replaced;
    }
    if (replaced == 0 || ebnf.find(std::to_string(kSentinel)) != std::string::npos)
        throw std::runtime_error("unexpected whitespace rule in xgrammar's schema grammar");
    return ebnf;
}

}  // namespace

struct GrammarContext {
    xgrammar::TokenizerInfo info;
    xgrammar::GrammarCompiler compiler;
    std::vector<uint32_t> forbidden;  // control tokens outside the EOG set
    std::mutex mutex;
    GrammarContext(const xgrammar::TokenizerInfo& tokenizer_info, int threads)
        : info(tokenizer_info), compiler(tokenizer_info, threads, true) {}
};

struct JsonConstraint {
    GrammarContext* context;
    xgrammar::GrammarMatcher matcher;
    std::vector<int32_t> bitmask;
    DLTensor tensor{};
    int64_t shape[1] = {0};
};

GrammarContext* GrammarContextCreate(const Tokenizer& tokenizer, std::string* error)
{
    try {
        std::vector<std::string> vocab(tokenizer.text.begin(), tokenizer.text.end());
        std::vector<int32_t> stops;
        for (size_t id = 0; id < tokenizer.eog.size(); ++id) if (tokenizer.eog[id]) stops.push_back(static_cast<int32_t>(id));
        const xgrammar::TokenizerInfo info(vocab, xgrammar::VocabType::BYTE_FALLBACK, static_cast<int>(vocab.size()), stops, false);
        const int threads = std::max(1, std::min(8, static_cast<int>(std::thread::hardware_concurrency())));
        auto* context = new GrammarContext(info, threads);
        context->forbidden.assign((vocab.size() + 31) / 32, 0);
        for (size_t id = 0; id < tokenizer.attrs.size(); ++id) {
            const bool control = tokenizer.attrs[id] & (kAttrControl | kAttrUnknown | kAttrUserDefined | kAttrUnused | kAttrUndefined);
            if ((control || tokenizer.attrs[id] == kAttrUndefined) && !tokenizer.eog[id]) context->forbidden[id >> 5] |= 1u << (id & 31);
        }
        return context;
    } catch (const std::exception& e) {
        *error = std::string("Grammar tokenizer setup failed: ") + e.what();
        return nullptr;
    }
}

void GrammarContextFree(GrammarContext* context) { delete context; }

JsonConstraint* JsonConstraintBegin(GrammarContext* context, const std::string& schema_text, std::string* error)
{
    try {
        json schema = schema_text.empty() ? json{{"type", "object"}} : json::parse(schema_text);
        NormalizeSchema(&schema);
        std::lock_guard<std::mutex> lock(context->mutex);
        const auto compiled = context->compiler.CompileGrammar(xgrammar::Grammar::FromEBNF(SchemaToEbnf(schema.dump())));
        auto* constraint = new JsonConstraint{context, xgrammar::GrammarMatcher(compiled), {}, {}, {}};
        const int vocab = context->info.GetVocabSize();
        constraint->bitmask.assign(static_cast<size_t>(xgrammar::GetBitmaskSize(vocab)), 0);
        constraint->shape[0] = static_cast<int64_t>(constraint->bitmask.size());
        constraint->tensor.data = constraint->bitmask.data();
        constraint->tensor.device = DLDevice{kDLCPU, 0};
        constraint->tensor.ndim = 1;
        constraint->tensor.dtype = DLDataType{kDLInt, 32, 1};
        constraint->tensor.shape = constraint->shape;
        return constraint;
    } catch (const std::exception& e) {
        *error = std::string("Invalid JSON schema constraint: ") + e.what();
        return nullptr;
    }
}

const uint32_t* JsonConstraintMask(JsonConstraint* constraint)
{
    constraint->matcher.FillNextTokenBitmask(&constraint->tensor);
    auto* words = reinterpret_cast<uint32_t*>(constraint->bitmask.data());
    const auto& forbidden = constraint->context->forbidden;
    for (size_t i = 0; i < forbidden.size(); ++i) words[i] &= ~forbidden[i];
    return words;
}

bool JsonConstraintAccept(JsonConstraint* constraint, int32_t token)
{
    return constraint->matcher.AcceptToken(token);
}

void JsonConstraintFree(JsonConstraint* constraint) { delete constraint; }

}  // namespace liminal::engine
