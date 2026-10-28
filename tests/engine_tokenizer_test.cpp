// Parity of the native GGUF reader, E4B binding, tokenizer and chat template with
// llama.cpp goldens frozen in tests/data/llm. Needs the game's GGUF; skips (77)
// when it is absent. LIMINAL_GOLDEN_DIR adds every full golden record.
#include "core/chat_format.h"
#include "core/e4b_spec.h"
#include "core/gguf.h"
#include "core/tokenizer.h"
#include "nlohmann/json.hpp"

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

static json Load(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return json::parse(in);
}

static std::string Excerpt(const std::string& text)
{
    std::string out = text.substr(0, 60);
    for (auto& c : out) if (c == '\n') c = '|';
    return out;
}

static void CheckPrompt(const Tokenizer& tokenizer, const json& messages, const std::string& expected_text,
    const std::vector<int32_t>& expected_tokens, const std::string& label)
{
    std::vector<ChatMessage> chat;
    for (const auto& m : messages) chat.push_back({m["role"].get<std::string>(), m["content"].get<std::string>()});
    const ChatPrompt prompt = RenderChat(tokenizer, chat);
    Check(prompt.text == expected_text, label + ": prompt text differs");
    if (prompt.tokens != expected_tokens) {
        size_t at = 0;
        while (at < prompt.tokens.size() && at < expected_tokens.size() && prompt.tokens[at] == expected_tokens[at]) ++at;
        Check(false, label + ": tokens differ at " + std::to_string(at) + " (native " + std::to_string(prompt.tokens.size()) +
            ", llama " + std::to_string(expected_tokens.size()) + ")");
    }
    std::string detokenized;
    for (size_t i = 1; i < prompt.tokens.size(); ++i) detokenized += TokenPiece(tokenizer, prompt.tokens[i]);
    Check(detokenized == expected_text, label + ": pieces do not reproduce the prompt");
}

int main()
{
    const char* env_model = std::getenv("LIMINAL_TEST_MODEL");
    const fs::path model = env_model ? fs::u8path(env_model)
        : fs::u8path(LIMINAL_SOURCE_DIR) / "models/gemma-4-e4b/gemma-4-E4B-it-Q4_K_M.gguf";
    if (!fs::is_regular_file(model)) { printf("SKIP: model not found at %s\n", model.u8string().c_str()); return 77; }
    const fs::path data = fs::u8path(LIMINAL_SOURCE_DIR) / "tests/data/llm";

    GgufFile file;
    std::string error;
    if (!GgufOpen(model.u8string(), &file, &error)) { fprintf(stderr, "FAIL: %s\n", error.c_str()); return 1; }
    E4bModel spec;
    Check(E4bBind(file, &spec, &error), "E4bBind: " + error);
    std::string pattern;
    for (const auto& layer : spec.layers) pattern += layer.swa ? 's' : 'F';
    printf("layers %s, shared sources %d/%d\n", pattern.c_str(), spec.layers[40].kv_source, spec.layers[41].kv_source);
    Check(!spec.layers[5].swa && spec.layers[4].swa && !spec.layers[41].swa, "full-attention layers are 5, 11, ..., 41");

    Tokenizer tokenizer;
    Check(TokenizerLoad(file, &tokenizer, &error), "TokenizerLoad: " + error);

    const json vocab = Load(data / "vocab.json");
    Check(tokenizer.text.size() == vocab["n_tokens"].get<size_t>(), "vocab size");
    Check(tokenizer.bos == vocab["bos"].get<int>() && tokenizer.eos == vocab["eos"].get<int>(), "bos/eos ids");
    std::vector<int32_t> eog;
    for (size_t id = 0; id < tokenizer.eog.size(); ++id) if (tokenizer.eog[id]) eog.push_back(static_cast<int32_t>(id));
    Check(eog == vocab["eog"].get<std::vector<int32_t>>(), "end-of-generation set");
    size_t id = 0;
    int attr_mismatch = 0;
    for (const auto& run : vocab["attr_runs"]) {
        for (int n = 0; n < run[1].get<int>(); ++n, ++id) {
            if (id < tokenizer.attrs.size() && tokenizer.attrs[id] != run[0].get<int>() && attr_mismatch++ < 5)
                fprintf(stderr, "attr %zu: native %d, llama %d (%s)\n", id, tokenizer.attrs[id], run[0].get<int>(), std::string(tokenizer.text[id]).c_str());
        }
    }
    Check(attr_mismatch == 0, "token attributes (" + std::to_string(attr_mismatch) + " differ)");

    int cases = 0;
    for (const auto& item : Load(data / "tokenizer_cases.json")) {
        const std::string text = item["text"];
        const bool parse_special = item["parse_special"];
        const auto tokens = Tokenize(tokenizer, text, false, parse_special);
        Check(tokens == item["tokens"].get<std::vector<int32_t>>(),
            "tokenize '" + Excerpt(text) + "' parse_special=" + (parse_special ? "1" : "0"));
        ++cases;
    }
    for (const auto& item : Load(data / "chat_cases.json")) {
        std::vector<ChatMessage> chat;
        for (const auto& m : item["messages"]) chat.push_back({m["role"].get<std::string>(), m["content"].get<std::string>()});
        const auto parts = RenderChatParts(chat);
        bool same = parts.size() == item["parts"].size();
        for (size_t i = 0; same && i < parts.size(); ++i)
            same = parts[i].text == item["parts"][i]["text"].get<std::string>() && parts[i].is_input == item["parts"][i]["is_input"].get<bool>();
        Check(same, "chat parts for " + item["messages"].dump().substr(0, 80));
        CheckPrompt(tokenizer, item["messages"], item["prompt_text"], item["tokens"].get<std::vector<int32_t>>(), "chat case");
        ++cases;
    }
    for (const auto& item : Load(data / "prompts.json")) {
        CheckPrompt(tokenizer, item["messages"], item["prompt_text"], item["tokens"].get<std::vector<int32_t>>(), item["purpose"]);
        ++cases;
    }
    if (const char* golden = std::getenv("LIMINAL_GOLDEN_DIR")) {
        const char* corpus = std::getenv("LIMINAL_CORPUS_DIR");
        for (const auto& entry : fs::directory_iterator(fs::u8path(golden))) {
            const std::string name = entry.path().filename().u8string();
            if (!corpus || name.find('-') == std::string::npos || entry.path().extension() != ".json") continue;
            const json record = Load(entry.path());
            if (!record.contains("record")) continue;
            const json source = Load(fs::u8path(corpus) / record["record"].get<std::string>());
            CheckPrompt(tokenizer, source["messages"], record["prompt_text"], record["tokens"].get<std::vector<int32_t>>(), name);
            ++cases;
        }
    }
    GgufClose(&file);
    printf("%d cases, %d failures\n", cases, g_failures);
    return g_failures ? 1 : 0;
}
