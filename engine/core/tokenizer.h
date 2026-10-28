#pragma once
// Gemma 4 tokenizer read from the GGUF, reproducing llama.cpp's llama-vocab.cpp
// (LLAMA_VOCAB_PRE_TYPE_GEMMA4): special-token partition, spaces escaped as U+2581,
// pre-split "[^\n]+|[\n]+", rank-ordered BPE merges on raw UTF-8, <0xXX> byte fallback.
#include "core/gguf.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace liminal::engine {

enum TokenAttr : int32_t {
    kAttrUndefined = 0, kAttrUnknown = 1 << 0, kAttrUnused = 1 << 1, kAttrNormal = 1 << 2,
    kAttrControl = 1 << 3, kAttrUserDefined = 1 << 4, kAttrByte = 1 << 5,
};

struct Tokenizer {
    std::vector<std::string_view> text;  // views into the mapped GGUF
    std::vector<int32_t> attrs;
    std::unordered_map<std::string_view, int32_t> text_to_id;
    std::unordered_map<std::string, int32_t> merge_rank;  // key: left + '\0' + right
    std::vector<int32_t> special;  // partition order (longest text first)
    std::vector<uint8_t> eog;      // per id
    std::vector<std::string> pieces;  // detokenized text per id (special tokens rendered)
    int32_t bos = -1;
    int32_t eos = -1;
    bool add_bos = true;
    bool add_eos = false;
};

bool TokenizerLoad(const GgufFile& file, Tokenizer* tokenizer, std::string* error);
// llama_tokenize(vocab, text, add_special, parse_special) equivalent.
std::vector<int32_t> Tokenize(const Tokenizer& tokenizer, const std::string& text, bool add_special, bool parse_special);
inline bool IsEog(const Tokenizer& tokenizer, int32_t id) { return id >= 0 && id < (int32_t)tokenizer.eog.size() && tokenizer.eog[id]; }
// llama_token_to_piece(..., special = true) equivalent.
inline const std::string& TokenPiece(const Tokenizer& tokenizer, int32_t id) { return tokenizer.pieces[static_cast<size_t>(id)]; }

}  // namespace liminal::engine
