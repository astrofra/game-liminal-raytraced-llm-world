#include "core/tokenizer.h"

#include <algorithm>
#include <cstring>
#include <queue>

namespace liminal::engine {

namespace {

static const char kMetaspace[] = "\xe2\x96\x81";  // U+2581

static size_t Utf8Length(char lead)
{
    static const size_t lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4};
    return lookup[static_cast<uint8_t>(lead) >> 4];
}

static void ReplaceAll(std::string* text, const std::string& from, const std::string& to)
{
    std::string result;
    result.reserve(text->size());
    size_t start = 0;
    for (size_t at; (at = text->find(from, start)) != std::string::npos; start = at + from.size()) {
        result.append(*text, start, at - start);
        result += to;
    }
    result.append(*text, start, std::string::npos);
    *text = std::move(result);
}

static std::string MergeKey(std::string_view left, std::string_view right)
{
    std::string key;
    key.reserve(left.size() + right.size() + 1);
    key.append(left);
    key.push_back('\0');
    key.append(right);
    return key;
}

struct Symbol {
    int prev;
    int next;
    const char* text;
    size_t n;
};

struct Bigram {
    int left;
    int right;
    std::string text;
    int rank;
};

// Pops the lowest rank first, then the leftmost (llm_bigram_bpe::comparator).
struct BigramOrder {
    bool operator()(const Bigram& l, const Bigram& r) const {
        return l.rank > r.rank || (l.rank == r.rank && l.left > r.left);
    }
};

struct BpeWord {
    const Tokenizer& tokenizer;
    std::vector<Symbol> symbols;
    std::priority_queue<Bigram, std::vector<Bigram>, BigramOrder> queue;

    void AddBigram(int left, int right) {
        if (left == -1 || right == -1) return;
        const std::string_view l(symbols[left].text, symbols[left].n);
        const std::string_view r(symbols[right].text, symbols[right].n);
        const auto it = tokenizer.merge_rank.find(MergeKey(l, r));
        if (it == tokenizer.merge_rank.end()) return;
        queue.push(Bigram{left, right, std::string(l) + std::string(r), it->second});
    }
};

// One pre-split word: either a newline run or a run without newlines.
static void TokenizeWord(const Tokenizer& tokenizer, const std::string& word, std::vector<Symbol>* final_symbols)
{
    BpeWord bpe{tokenizer, {}, {}};
    size_t offset = 0;
    if (word.find_first_not_of('\n') == std::string::npos && tokenizer.text_to_id.count(word)) {
        bpe.symbols.push_back(Symbol{-1, -1, word.c_str(), word.size()});
        offset = word.size();
    }
    int index = 0;
    while (offset < word.size()) {
        Symbol sym;
        const size_t char_len = std::min(word.size() - offset, Utf8Length(word[offset]));
        sym.text = word.c_str() + offset;
        sym.n = char_len;
        offset += sym.n;
        sym.prev = index - 1;
        sym.next = offset == word.size() ? -1 : index + 1;
        index++;
        bpe.symbols.push_back(sym);
    }
    for (int i = 1; i < static_cast<int>(bpe.symbols.size()); ++i) bpe.AddBigram(i - 1, i);
    while (!bpe.queue.empty()) {
        Bigram bigram = bpe.queue.top();
        bpe.queue.pop();
        Symbol& left = bpe.symbols[bigram.left];
        Symbol& right = bpe.symbols[bigram.right];
        if (left.n == 0 || right.n == 0) continue;
        if (std::string(left.text, left.n) + std::string(right.text, right.n) != bigram.text) continue;
        left.n += right.n;
        right.n = 0;
        left.next = right.next;
        if (right.next >= 0) bpe.symbols[right.next].prev = bigram.left;
        bpe.AddBigram(left.prev, bigram.left);
        bpe.AddBigram(bigram.left, left.next);
    }
    for (const Symbol& sym : bpe.symbols)
        if (sym.n > 0) final_symbols->push_back(sym);
}

static void TokenizeText(const Tokenizer& tokenizer, std::string text, std::vector<int32_t>* output)
{
    ReplaceAll(&text, " ", kMetaspace);
    // "[^\n]+|[\n]+": alternating runs of non-newlines and newlines.
    std::vector<std::string> words;
    for (size_t start = 0; start < text.size();) {
        const bool newline = text[start] == '\n';
        size_t end = start;
        while (end < text.size() && (text[end] == '\n') == newline) ++end;
        words.push_back(text.substr(start, end - start));
        start = end;
    }
    // Symbols point into `words`, which stays alive until the ids are emitted.
    std::vector<Symbol> symbols;
    for (const auto& word : words) TokenizeWord(tokenizer, word, &symbols);
    static const char* hex = "0123456789ABCDEF";
    for (const Symbol& symbol : symbols) {
        const std::string_view str(symbol.text, symbol.n);
        const auto it = tokenizer.text_to_id.find(str);
        if (it != tokenizer.text_to_id.end()) { output->push_back(it->second); continue; }
        for (char c : str) {
            const uint8_t ch = static_cast<uint8_t>(c);
            const char buf[7] = {'<', '0', 'x', hex[ch >> 4], hex[ch & 15], '>', 0};
            const auto byte = tokenizer.text_to_id.find(std::string_view(buf, 6));
            if (byte != tokenizer.text_to_id.end()) output->push_back(byte->second);
        }
    }
}

struct Fragment {
    int32_t token;  // -1 for raw text
    size_t offset;
    size_t length;
};

}  // namespace

bool TokenizerLoad(const GgufFile& file, Tokenizer* tokenizer, std::string* error)
{
    const GgufKv* tokens = GgufFind(file, "tokenizer.ggml.tokens");
    const GgufKv* types = GgufFind(file, "tokenizer.ggml.token_type");
    const GgufKv* merges = GgufFind(file, "tokenizer.ggml.merges");
    if (!tokens || tokens->type != kGgufArray || tokens->array_type != kGgufString || tokens->array_strings.empty()) {
        *error = "Missing tokenizer.ggml.tokens"; return false;
    }
    if (!types || types->type != kGgufArray || types->array_type != kGgufInt32 || types->array_count != tokens->array_strings.size()) {
        *error = "Missing tokenizer.ggml.token_type"; return false;
    }
    if (!merges || merges->type != kGgufArray || merges->array_type != kGgufString) { *error = "Missing tokenizer.ggml.merges"; return false; }
    const size_t n = tokens->array_strings.size();
    Tokenizer& t = *tokenizer;
    t = Tokenizer();
    t.text = tokens->array_strings;
    t.attrs.resize(n);
    t.text_to_id.reserve(n);
    for (size_t id = 0; id < n; ++id) {
        int32_t type = 0;
        memcpy(&type, types->array_data + id * 4, 4);
        switch (type) {
            case 1: t.attrs[id] = kAttrNormal; break;
            case 2: t.attrs[id] = kAttrUnknown; break;
            case 3: t.attrs[id] = kAttrControl; break;
            case 4: t.attrs[id] = kAttrUserDefined; break;
            case 5: t.attrs[id] = kAttrUnused; break;
            case 6: t.attrs[id] = kAttrByte; break;
            default: t.attrs[id] = kAttrUndefined; break;
        }
        // A later duplicate text keeps the last id, as llama.cpp's token_to_id[text] = id.
        t.text_to_id[t.text[id]] = static_cast<int32_t>(id);
    }

    t.merge_rank.reserve(merges->array_strings.size());
    for (size_t i = 0; i < merges->array_strings.size(); ++i) {
        const std::string_view word = merges->array_strings[i];
        const size_t pos = word.find(' ', 1);
        std::string_view first, second;
        if (pos != std::string_view::npos) { first = word.substr(0, pos); second = word.substr(pos + 1); }
        t.merge_rank.emplace(MergeKey(first, second), static_cast<int32_t>(i));
    }

    uint32_t value = 0;
    if (GgufGetU32(file, "tokenizer.ggml.bos_token_id", &value)) t.bos = static_cast<int32_t>(value);
    if (GgufGetU32(file, "tokenizer.ggml.eos_token_id", &value)) t.eos = static_cast<int32_t>(value);
    bool flag = false;
    if (GgufGetBool(file, "tokenizer.ggml.add_eos_token", &flag)) t.add_eos = flag;
    t.add_bos = true;  // llama.cpp forces add_bos for Gemma 4
    if (t.bos < 0 || t.bos >= (int32_t)n || t.eos < 0 || t.eos >= (int32_t)n) { *error = "Invalid BOS/EOS ids"; return false; }

    // Control tokens named "unused" are unused (llama-vocab auto-detection).
    for (size_t id = 0; id < n; ++id)
        if ((t.attrs[id] & kAttrControl) && !(t.attrs[id] & kAttrUnused) && t.text[id].find("unused") != std::string_view::npos)
            t.attrs[id] |= kAttrUnused;
    // End-of-generation: EOS plus the Gemma 4 turn/tool markers, forced to control.
    t.eog.assign(n, 0);
    t.eog[static_cast<size_t>(t.eos)] = 1;
    for (const char* name : {"<eos>", "<turn|>", "<|tool_response>"}) {
        const auto it = t.text_to_id.find(name);
        if (it == t.text_to_id.end()) continue;
        t.eog[static_cast<size_t>(it->second)] = 1;
        t.attrs[static_cast<size_t>(it->second)] |= kAttrControl;
    }

    for (size_t id = 0; id < n; ++id)
        if (t.attrs[id] & (kAttrControl | kAttrUserDefined | kAttrUnknown)) t.special.push_back(static_cast<int32_t>(id));
    std::sort(t.special.begin(), t.special.end(), [&](int32_t a, int32_t b) { return t.text[a].size() > t.text[b].size(); });

    t.pieces.resize(n);
    for (size_t id = 0; id < n; ++id) {
        const int32_t attr = t.attrs[id];
        if (attr & (kAttrUnknown | kAttrControl | kAttrUserDefined)) {
            t.pieces[id] = std::string(t.text[id]);
        } else if (attr & kAttrNormal) {
            t.pieces[id] = std::string(t.text[id]);
            ReplaceAll(&t.pieces[id], kMetaspace, " ");
        } else if ((attr & kAttrByte) && t.text[id].size() >= 6) {
            t.pieces[id] = std::string(1, static_cast<char>(strtol(std::string(t.text[id].substr(3, 2)).c_str(), nullptr, 16)));
        }
    }
    return true;
}

std::vector<int32_t> Tokenize(const Tokenizer& tokenizer, const std::string& text, bool add_special, bool parse_special)
{
    std::vector<Fragment> fragments;
    if (!text.empty()) fragments.push_back(Fragment{-1, 0, text.size()});
    for (const int32_t special : tokenizer.special) {
        if (!parse_special && (tokenizer.attrs[static_cast<size_t>(special)] & (kAttrControl | kAttrUnknown))) continue;
        const std::string_view needle = tokenizer.text[static_cast<size_t>(special)];
        if (needle.empty()) continue;
        std::vector<Fragment> next;
        next.reserve(fragments.size());
        for (const Fragment& fragment : fragments) {
            if (fragment.token >= 0) { next.push_back(fragment); continue; }
            const std::string_view raw(text.data(), fragment.offset + fragment.length);
            size_t base = fragment.offset;
            size_t length = fragment.length;
            while (true) {
                const size_t match = raw.find(needle, base);
                if (match == std::string_view::npos) { next.push_back(Fragment{-1, base, length}); break; }
                if (match > base) next.push_back(Fragment{-1, base, match - base});
                next.push_back(Fragment{special, 0, 0});
                const size_t right = match + needle.size();
                if (right >= base + length) break;
                length = base + length - right;
                base = right;
            }
        }
        fragments = std::move(next);
    }
    std::vector<int32_t> output;
    if (add_special && tokenizer.add_bos) output.push_back(tokenizer.bos);
    for (const Fragment& fragment : fragments) {
        if (fragment.token >= 0) output.push_back(fragment.token);
        else TokenizeText(tokenizer, text.substr(fragment.offset, fragment.length), &output);
    }
    if (add_special && tokenizer.add_eos) output.push_back(tokenizer.eos);
    return output;
}

}  // namespace liminal::engine
