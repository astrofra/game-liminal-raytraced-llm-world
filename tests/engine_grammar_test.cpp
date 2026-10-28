// The native JSON constraint must admit every token llama.cpp's grammar admitted
// for the game's real schemas (tests/data/llm/prompts.json), and reject values a
// schema forbids. Needs the game's GGUF; skips (77) when it is absent.
#include "core/gguf.h"
#include "core/grammar.h"
#include "core/tokenizer.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
using namespace liminal::engine;
namespace fs = std::filesystem;

static int g_failures = 0;

static void Check(bool ok, const std::string& what)
{
    if (!ok) { ++g_failures; fprintf(stderr, "FAIL: %s\n", what.c_str()); }
}

static bool Allowed(const uint32_t* mask, int32_t id) { return (mask[id >> 5] >> (id & 31)) & 1u; }

// Feeds tokens through a fresh constraint; returns the index of the first refused token or -1.
static int Walk(GrammarContext* grammar, const std::string& schema, const std::vector<int32_t>& tokens, std::string* error)
{
    JsonConstraint* constraint = JsonConstraintBegin(grammar, schema, error);
    if (!constraint) return 0;
    int refused = -1;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const uint32_t* mask = JsonConstraintMask(constraint);
        if (!Allowed(mask, tokens[i]) || !JsonConstraintAccept(constraint, tokens[i])) { refused = static_cast<int>(i); break; }
    }
    JsonConstraintFree(constraint);
    return refused;
}

int main()
{
    const char* env_model = std::getenv("LIMINAL_TEST_MODEL");
    const fs::path model = env_model ? fs::u8path(env_model)
        : fs::u8path(LIMINAL_SOURCE_DIR) / "models/gemma-4-e4b/gemma-4-E4B-it-Q4_K_M.gguf";
    if (!fs::is_regular_file(model)) { printf("SKIP: model not found at %s\n", model.u8string().c_str()); return 77; }
    GgufFile file;
    Tokenizer tokenizer;
    std::string error;
    if (!GgufOpen(model.u8string(), &file, &error) || !TokenizerLoad(file, &tokenizer, &error)) { fprintf(stderr, "FAIL: %s\n", error.c_str()); return 1; }
    const auto start = std::chrono::steady_clock::now();
    GrammarContext* grammar = GrammarContextCreate(tokenizer, &error);
    Check(grammar != nullptr, "grammar context: " + error);
    if (!grammar) return 1;
    printf("grammar context: %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());

    std::ifstream in(fs::u8path(LIMINAL_SOURCE_DIR) / "tests/data/llm/prompts.json", std::ios::binary);
    for (const auto& item : json::parse(in)) {
        if (!item["use_json_grammar"].get<bool>()) continue;
        const std::string purpose = item["purpose"];
        const auto tokens = item["greedy"].get<std::vector<int32_t>>();
        const auto t0 = std::chrono::steady_clock::now();
        const int refused = Walk(grammar, item["json_schema"], tokens, &error);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        Check(refused < 0, purpose + ": llama.cpp's constrained token " + std::to_string(refused) + " refused " + error);
        printf("%s: %zu tokens (%s) admitted in %.0f ms\n", purpose.c_str(), tokens.size(), item["greedy_stop"].get<std::string>().c_str(), ms);
    }

    // verify_native_llm.py's schema: only {"ok":true}.
    const std::string schema = R"({"type":"object","properties":{"ok":{"const":true}},"required":["ok"],"additionalProperties":false})";
    std::vector<int32_t> good = Tokenize(tokenizer, "{\"ok\":true}", false, false);
    good.push_back(tokenizer.eos);
    Check(Walk(grammar, schema, good, &error) < 0, "const schema accepts {\"ok\":true}");
    Check(Walk(grammar, schema, Tokenize(tokenizer, "{\"ok\":false}", false, false), &error) >= 0, "const schema rejects false");
    Check(Walk(grammar, schema, Tokenize(tokenizer, "{\"ok\":true,\"extra\":1}", false, false), &error) >= 0, "no additional properties");
    std::vector<int32_t> any = Tokenize(tokenizer, "{\"free\": [1, \"x\", {\"y\": null}]}", false, false);
    any.push_back(tokenizer.eos);
    Check(Walk(grammar, "", any, &error) < 0, "empty schema accepts any object");
    Check(Walk(grammar, "", Tokenize(tokenizer, "[1]", false, false), &error) >= 0, "empty schema rejects a top-level array");
    const auto control = Tokenize(tokenizer, "{\"a\":\"<|turn>\"}", false, true);
    Check(Walk(grammar, R"({"type":"object","properties":{"a":{"type":"string"}}})", control, &error) >= 0, "control tokens refused inside strings");

    GrammarContextFree(grammar);
    GgufClose(&file);
    printf("%d failures\n", g_failures);
    return g_failures ? 1 : 0;
}
