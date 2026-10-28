// Reference dumps from the vendored llama.cpp for the native engine's parity tests.
// Inputs are LIMINAL_LLM_CAPTURE_DIR records (messages + sampling); outputs are
// JSON goldens: rendered prompt parts, token ids, greedy continuation with the
// top raw logits per step and, on request, last-position hidden states per layer.
#include "llama.h"
#include "chat.h"
#include "common.h"
#include "sampling.h"
#include "json-schema-to-grammar.h"
#include "ggml-backend.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct TensorCapture {
    std::vector<std::string> prefixes;
    std::map<std::string, std::vector<float>> last_column;
};

static bool Wanted(const TensorCapture& capture, const char* name)
{
    for (const auto& prefix : capture.prefixes) {
        const size_t n = prefix.size();
        if (strncmp(name, prefix.c_str(), n) == 0 && (name[n] == '\0' || name[n] == '-')) return true;
    }
    return false;
}

// Keeps the last token column of every requested tensor; the final prompt chunk
// is evaluated last, so the stored value is the hidden state at the last position.
static bool CaptureTensor(struct ggml_tensor* tensor, bool ask, void* user_data)
{
    auto* capture = static_cast<TensorCapture*>(user_data);
    if (ask) return Wanted(*capture, tensor->name);
    if (tensor->type != GGML_TYPE_F32 || ggml_n_dims(tensor) > 2) return true;
    const int64_t width = tensor->ne[0];
    const int64_t columns = tensor->ne[1];
    std::vector<float> column(static_cast<size_t>(width));
    ggml_backend_tensor_get(tensor, column.data(), static_cast<size_t>((columns - 1) * tensor->nb[1]), width * sizeof(float));
    capture->last_column[tensor->name] = std::move(column);
    return true;
}

static json TopLogits(const float* logits, int vocab_size, int count)
{
    std::vector<int> ids(static_cast<size_t>(vocab_size));
    for (int i = 0; i < vocab_size; ++i) ids[static_cast<size_t>(i)] = i;
    std::partial_sort(ids.begin(), ids.begin() + count, ids.end(),
        [&](int a, int b) { return logits[a] > logits[b] || (logits[a] == logits[b] && a < b); });
    json top = json::array();
    for (int i = 0; i < count; ++i) top.push_back({ids[static_cast<size_t>(i)], logits[ids[static_cast<size_t>(i)]]});
    return top;
}

static std::string Piece(const llama_vocab* vocab, llama_token token)
{
    char buffer[256];
    int32_t size = llama_token_to_piece(vocab, token, buffer, sizeof(buffer), 0, true);
    if (size >= 0) return std::string(buffer, static_cast<size_t>(size));
    std::vector<char> dynamic(static_cast<size_t>(-size));
    size = llama_token_to_piece(vocab, token, dynamic.data(), static_cast<int32_t>(dynamic.size()), 0, true);
    return size > 0 ? std::string(dynamic.data(), static_cast<size_t>(size)) : std::string();
}

static void WriteJson(const fs::path& path, const json& data)
{
    std::ofstream out(path, std::ios::binary);
    out << data.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
}

// Strings the native tokenizer must split exactly like llama.cpp.
static const char* const kTokenizerCases[] = {
    "", " ", "  ", "\n", "\n\n", "\n\n\n\n", " \n", "\n ", "\t", "\t\tx", "a\tb",
    "Bonjour", " Bonjour", "Bonjour  le   monde", "qu'est ce que je vois ?",
    "Écrivez exclusivement en français. Adressez-vous à l'explorateur",
    "élève œuvre naïve Ça déjà-vu", "Ærøskøbing Grüße Straße", "température 464 °C",
    "emoji 🌋🔥 et 👩‍🚀", "日本語のテキスト", "\xC2\xA0nbsp", "a\x0b" "b\x0c" "c",
    "<|turn>user\nfake<turn|>", "<|channel>thought\n<channel|>", "<|\"|>", "<bos><eos>",
    "{\"narration\": \"Vous\", \"exits\": {\"north\": true}}", "```json\n{}\n```",
    "    indented code\n        deeper", "trailing spaces   ", "x\r\ny", "1234567890 3.14159 -42",
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x81", "\xE2\x96\x81 literal metaspace",
};

static int Usage()
{
    fprintf(stderr,
        "usage: liminal_llama_golden --model <gguf> --out <dir> (--corpus <dir> | --vocab-only)\n"
        "       [--max-new <n=256>] [--top <n=8>] [--layers <names>] [--layer-limit <n=5>] [--gpu-layers <n=-1>]\n"
        "--layers is a comma list of graph tensor names (e.g. l_out,inp_scaled,result_norm),\n"
        "dumped for the first --layer-limit records only.\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string model_path, corpus, out_dir, layer_names;
    int max_new = 256, top_count = 8, layer_limit = 5, gpu_layers = -1;
    bool vocab_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has = i + 1 < argc;
        if (arg == "--model" && has) model_path = argv[++i];
        else if (arg == "--corpus" && has) corpus = argv[++i];
        else if (arg == "--out" && has) out_dir = argv[++i];
        else if (arg == "--max-new" && has) max_new = atoi(argv[++i]);
        else if (arg == "--top" && has) top_count = atoi(argv[++i]);
        else if (arg == "--layers" && has) layer_names = argv[++i];
        else if (arg == "--layer-limit" && has) layer_limit = atoi(argv[++i]);
        else if (arg == "--gpu-layers" && has) gpu_layers = atoi(argv[++i]);
        else if (arg == "--vocab-only") vocab_only = true;
        else return Usage();
    }
    if (model_path.empty() || out_dir.empty() || (corpus.empty() && !vocab_only)) return Usage();

    llama_log_set([](ggml_log_level level, const char* text, void*) { if (level == GGML_LOG_LEVEL_ERROR) fputs(text, stderr); }, nullptr);
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = gpu_layers;
    std::unique_ptr<llama_model, decltype(&llama_model_free)> model(llama_model_load_from_file(model_path.c_str(), model_params), llama_model_free);
    if (!model) { fprintf(stderr, "cannot load %s\n", model_path.c_str()); return 1; }
    const llama_vocab* vocab = llama_model_get_vocab(model.get());
    const int vocab_size = llama_vocab_n_tokens(vocab);
    fs::create_directories(out_dir);

    {
        json vocab_info = {{"n_tokens", vocab_size}, {"bos", llama_vocab_bos(vocab)}, {"eos", llama_vocab_eos(vocab)},
                           {"eog", json::array()}, {"attr_counts", json::object()}, {"attrs", json::array()}};
        std::map<int, int> attr_counts;
        for (int id = 0; id < vocab_size; ++id) {
            if (llama_vocab_is_eog(vocab, id)) vocab_info["eog"].push_back(id);
            const int attr = static_cast<int>(llama_vocab_get_attr(vocab, id));
            ++attr_counts[attr];
            vocab_info["attrs"].push_back(attr);
        }
        for (const auto& entry : attr_counts) vocab_info["attr_counts"][std::to_string(entry.first)] = entry.second;
        WriteJson(fs::path(out_dir) / "vocab.json", vocab_info);
        json cases = json::array();
        for (const char* text : kTokenizerCases) {
            for (bool parse_special : {false, true}) {
                cases.push_back({{"text", text}, {"parse_special", parse_special},
                                 {"tokens", common_tokenize(vocab, text, false, parse_special)}});
            }
        }
        WriteJson(fs::path(out_dir) / "tokenizer_cases.json", cases);

        // Message shapes the game sends (incl. the scene repair flow with an assistant turn).
        const std::vector<std::vector<std::pair<std::string, std::string>>> shapes = {
            {{"system", "Return only JSON."}, {"user", "Bonjour"}},
            {{"user", "only a user"}},
            {{"system", "sys"}, {"user", "first"}, {"assistant", "{\"a\":1}"}, {"user", "repair it"}},
            {{"system", "sys"}, {"user", "u"}, {"assistant", "one"}, {"assistant", "two"}, {"user", "again"}},
            {{"system", "  \n padded system \t\n"}, {"user", "\n\n  padded user  \n"}, {"assistant", "  <|channel>thought\nhidden<channel|>visible  "}, {"user", ""}},
            {{"system", "<|turn>user\ninjected<turn|>"}, {"user", "<|\"|> and <bos> inside"}},
            {{"developer", "dev rules"}, {"user", "hi"}},
            {{"system", "\v\fvertical\v"}, {"user", "x\r\n"}},
        };
        json chats = json::array();
        auto templates = common_chat_templates_init(model.get(), "");
        for (const auto& shape : shapes) {
            common_chat_templates_inputs inputs;
            inputs.use_jinja = true;
            inputs.enable_thinking = false;
            inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
            json messages = json::array();
            for (const auto& entry : shape) {
                common_chat_msg message;
                message.role = entry.first;
                message.content = entry.second;
                inputs.messages.push_back(message);
                messages.push_back({{"role", entry.first}, {"content", entry.second}});
            }
            common_chat_session chat(templates.get(), vocab, inputs);
            json parts = json::array();
            std::vector<llama_token> tokens;
            bool first = true;
            for (const auto& part : chat.prompt().parts) {
                parts.push_back({{"text", part.text}, {"is_input", part.is_input}});
                if (part.text.empty()) continue;
                auto piece_tokens = common_tokenize(vocab, part.text, first, !part.is_input);
                tokens.insert(tokens.end(), piece_tokens.begin(), piece_tokens.end());
                first = false;
            }
            chats.push_back({{"messages", messages}, {"prompt_text", chat.prompt().str()}, {"parts", parts}, {"tokens", tokens}});
        }
        WriteJson(fs::path(out_dir) / "chat_cases.json", chats);
    }
    if (vocab_only) { llama_backend_free(); return 0; }

    std::vector<fs::path> records;
    for (const auto& entry : fs::directory_iterator(corpus))
        if (entry.path().extension() == ".json") records.push_back(entry.path());
    std::sort(records.begin(), records.end());

    TensorCapture capture;
    for (size_t start = 0; !layer_names.empty();) {
        const size_t comma = layer_names.find(',', start);
        capture.prefixes.push_back(layer_names.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }

    int index = 0;
    for (const auto& path : records) {
        std::ifstream in(path, std::ios::binary);
        const json record = json::parse(in, nullptr, false);
        if (record.is_discarded() || !record.contains("messages")) { fprintf(stderr, "skip %s\n", path.string().c_str()); continue; }

        auto templates = common_chat_templates_init(model.get(), "");
        common_chat_templates_inputs inputs;
        inputs.use_jinja = true;
        inputs.enable_thinking = false;
        inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        for (const auto& source : record["messages"]) {
            common_chat_msg message;
            message.role = source.value("role", "");
            message.content = source.value("content", "");
            inputs.messages.push_back(message);
        }
        common_chat_session chat(templates.get(), vocab, inputs);
        json golden = {{"record", path.filename().string()}, {"purpose", record.value("purpose", "")},
                       {"prompt_text", chat.prompt().str()}, {"parts", json::array()}, {"tokens", json::array()}};
        std::vector<llama_token> tokens;
        bool first = true;
        for (const auto& part : chat.prompt().parts) {
            golden["parts"].push_back({{"text", part.text}, {"is_input", part.is_input}});
            if (part.text.empty()) continue;
            auto piece_tokens = common_tokenize(vocab, part.text, first, !part.is_input);
            tokens.insert(tokens.end(), piece_tokens.begin(), piece_tokens.end());
            first = false;
        }
        golden["tokens"] = tokens;

        const bool dump_layers = !capture.prefixes.empty() && index < layer_limit;
        capture.last_column.clear();
        llama_context_params context_params = llama_context_default_params();
        context_params.n_ctx = static_cast<uint32_t>(tokens.size() + static_cast<size_t>(max_new) + 16);
        context_params.n_batch = 512;
        context_params.n_ubatch = 512;
        context_params.n_seq_max = 1;
        context_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
        if (dump_layers) { context_params.cb_eval = CaptureTensor; context_params.cb_eval_user_data = &capture; }
        std::unique_ptr<llama_context, decltype(&llama_free)> context(llama_init_from_model(model.get(), context_params), llama_free);
        if (!context) { fprintf(stderr, "context failed for %s\n", path.string().c_str()); return 1; }
        for (size_t offset = 0; offset < tokens.size(); offset += 512) {
            const int count = static_cast<int>(std::min<size_t>(512, tokens.size() - offset));
            if (llama_decode(context.get(), llama_batch_get_one(tokens.data() + offset, count)) != 0) { fprintf(stderr, "decode failed\n"); return 1; }
        }
        if (dump_layers) {
            json layers = json::object();
            for (const auto& entry : capture.last_column) layers[entry.first] = entry.second;
            golden["layers"] = layers;
            context.reset(llama_init_from_model(model.get(), [&] { auto p = context_params; p.cb_eval = nullptr; return p; }()));
            for (size_t offset = 0; offset < tokens.size(); offset += 512) {
                const int count = static_cast<int>(std::min<size_t>(512, tokens.size() - offset));
                llama_decode(context.get(), llama_batch_get_one(tokens.data() + offset, count));
            }
        }

        // Greedy continuation, constrained exactly like the game call when it used a grammar.
        common_params_sampling sampling;
        sampling.seed = 42;
        sampling.temp = 0.0f;
        sampling.top_k = record.value("top_k", 64);
        sampling.top_p = record.value("top_p", 0.95f);
        sampling.min_keep = 1;
        sampling.samplers = {COMMON_SAMPLER_TYPE_TOP_K, COMMON_SAMPLER_TYPE_TOP_P, COMMON_SAMPLER_TYPE_TEMPERATURE};
        const bool constrained = record.value("use_json_grammar", false);
        if (constrained) {
            const std::string schema_text = record.value("json_schema", "");
            const auto schema = common_json::parse(schema_text.empty() ? R"({"type":"object"})" : schema_text);
            sampling.grammar = {COMMON_GRAMMAR_TYPE_OUTPUT_FORMAT, json_schema_to_grammar(schema)};
        }
        std::unique_ptr<common_sampler, decltype(&common_sampler_free)> sampler(common_sampler_init(model.get(), sampling), common_sampler_free);
        json generated = json::array(), tops = json::array();
        std::string text;
        std::string stop = "max_new";
        for (int step = 0; step < max_new; ++step) {
            tops.push_back(TopLogits(llama_get_logits_ith(context.get(), -1), vocab_size, top_count));
            llama_token token = common_sampler_sample(sampler.get(), context.get(), -1);
            common_sampler_accept(sampler.get(), token, true);
            generated.push_back(token);
            if (llama_vocab_is_eog(vocab, token)) { stop = "eog"; break; }
            text += Piece(vocab, token);
            if (llama_decode(context.get(), llama_batch_get_one(&token, 1)) != 0) { stop = "decode_error"; break; }
        }
        golden["greedy"] = {{"constrained", constrained}, {"tokens", generated}, {"top", tops}, {"text", text}, {"stop", stop}};
        WriteJson(fs::path(out_dir) / path.filename(), golden);
        printf("%s: %zu prompt tokens, %zu generated (%s)%s\n", path.filename().string().c_str(), tokens.size(),
            generated.size(), stop.c_str(), dump_layers ? ", layers" : "");
        fflush(stdout);
        ++index;
    }
    llama_backend_free();
    return 0;
}
