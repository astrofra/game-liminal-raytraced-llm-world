#include "scene_normalization.h"

#include <cmath>
#include <cstdlib>

namespace liminal {
namespace {

constexpr size_t kMaxSceneBytes = 65536;
constexpr char kByteOrderMark[] = "\xEF\xBB\xBF";
constexpr size_t kByteOrderMarkLength = 3;
constexpr char kFence[] = "```";

bool IsBlank(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

bool IsInlineSpace(char c) { return c == ' ' || c == '\t'; }

bool IsIdentifierStart(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }

bool IsIdentifierChar(char c) { return IsIdentifierStart(c) || (c >= '0' && c <= '9'); }

// A quoted name may contain '#', ';' or parentheses; those stay untouched.
size_t FindCommentStart(const std::string& line) {
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') quoted = !quoted;
        else if (c == '#' && !quoted) return i;
    }
    return std::string::npos;
}

bool IsQuotedAt(const std::string& line, size_t index) {
    bool quoted = false;
    for (size_t i = 0; i < index && i < line.size(); ++i) {
        if (line[i] == '"') quoted = !quoted;
    }
    return quoted;
}

bool NormalizeNewlines(std::string* text) {
    bool changed = false;
    std::string normalized;
    normalized.reserve(text->size());
    for (size_t i = 0; i < text->size(); ++i) {
        const char c = (*text)[i];
        if (c == '\r') {
            changed = true;
            normalized += '\n';
            if (i + 1 < text->size() && (*text)[i + 1] == '\n') ++i;
        } else {
            normalized += c;
        }
    }
    if (changed) *text = std::move(normalized);
    return changed;
}

// Accept exactly one complete fenced block surrounded by whitespace only.
// Prose, partial fences and nested fences are left alone.
bool TryUnwrapSingleEnvelope(const std::string& text, std::string* inner) {
    size_t begin = 0;
    while (begin < text.size() && IsBlank(text[begin])) ++begin;
    if (begin >= text.size()) return false;
    size_t last = text.size() - 1;
    while (last > begin && IsBlank(text[last])) --last;
    if (text.compare(begin, 3, kFence) != 0) return false;
    const size_t label_line = text.find('\n', begin);
    if (label_line == std::string::npos) return false;
    std::string label = text.substr(begin, label_line - begin);
    while (!label.empty() && IsInlineSpace(label.back())) label.pop_back();
    if (label != "```" && label != "```scene" && label != "```.scene") return false;
    if (last < 3) return false;
    const size_t close = last - 2;
    if (close <= label_line + 1) return false;
    if (text.compare(close, 3, kFence) != 0) return false;
    if (text[close - 1] != '\n') return false;
    if (text.find(kFence, label_line + 1) != close) return false;
    inner->assign(text, label_line + 1, close - label_line - 1);
    return true;
}

// Exactly one terminator after a nonempty directive; never erase an invalid line.
bool RemoveTerminalSemicolons(const std::string& line, std::string* out) {
    const size_t comment = FindCommentStart(line);
    const size_t code_end = comment == std::string::npos ? line.size() : comment;
    std::vector<size_t> removals;
    size_t cursor = code_end;
    for (;;) {
        while (cursor > 0 && IsInlineSpace(line[cursor - 1])) --cursor;
        if (cursor == 0) break;
        if (line[cursor - 1] != ';' || IsQuotedAt(line, cursor - 1)) break;
        removals.push_back(cursor - 1);
        --cursor;
    }
    if (removals.size()!=1 || cursor==0) return false;
    *out = line;
    for (const size_t index : removals) out->erase(index, 1);
    return true;
}

bool IsFiniteNumberToken(const std::string& token) {
    if (token.empty()) return false;
    const char* begin = token.c_str();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0') return false;
    return std::isfinite(value);
}

// A list of one or more comma-separated finite numbers, no empty item.
bool IsFiniteNumberList(const std::string& content) {
    if (content.empty()) return false;
    size_t start = 0;
    bool any = false;
    for (;;) {
        const size_t comma = content.find(',', start);
        std::string token = comma == std::string::npos ? content.substr(start)
                                                       : content.substr(start, comma - start);
        size_t first = 0, stop = token.size();
        while (first < stop && IsInlineSpace(token[first])) ++first;
        while (stop > first && IsInlineSpace(token[stop - 1])) --stop;
        if (first == stop) return false;
        if (!IsFiniteNumberToken(token.substr(first, stop - first))) return false;
        any = true;
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return any;
}

// Property lists are flat: any nesting, imbalance or stray quote on the line
// makes the whole line ineligible rather than guessing a repair.
bool HasFlatParentheses(const std::string& line, size_t code_end) {
    bool quoted = false;
    int depth = 0;
    for (size_t i = 0; i < code_end; ++i) {
        const char c = line[i];
        if (c == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (c == '(') {
            ++depth;
            if (depth > 1) return false;
        } else if (c == ')') {
            --depth;
            if (depth < 0) return false;
        }
    }
    return depth == 0 && !quoted;
}

bool RemoveTrailingListCommas(const std::string& line, std::string* out) {
    const size_t comment = FindCommentStart(line);
    const size_t code_end = comment == std::string::npos ? line.size() : comment;
    if (!HasFlatParentheses(line, code_end)) return false;
    std::vector<size_t> removals;
    bool quoted = false;
    for (size_t i = 0; i < code_end; ++i) {
        const char c = line[i];
        if (c == '"') { quoted = !quoted; continue; }
        if (quoted || c != '(') continue;
        size_t key_begin = i;
        while (key_begin > 0 && IsIdentifierChar(line[key_begin - 1])) --key_begin;
        if (key_begin == i || !IsIdentifierStart(line[key_begin])) continue;
        const size_t close = line.find(')', i + 1);
        if (close == std::string::npos || close >= code_end) continue;
        const std::string content = line.substr(i + 1, close - i - 1);
        size_t stop = content.size();
        while (stop > 0 && IsInlineSpace(content[stop - 1])) --stop;
        if (stop == 0 || content[stop - 1] != ',') continue;
        if (!IsFiniteNumberList(content.substr(0, stop - 1))) continue;
        removals.push_back(i + stop);
    }
    if (removals.empty()) return false;
    std::string rebuilt;
    rebuilt.reserve(line.size() - removals.size());
    size_t previous = 0;
    for (const size_t index : removals) {
        rebuilt.append(line, previous, index - previous);
        previous = index + 1;
    }
    rebuilt.append(line, previous, std::string::npos);
    *out = std::move(rebuilt);
    return true;
}

} // namespace

SceneNormalization NormalizeSceneSyntax(const std::string& raw) {
    SceneNormalization result;
    result.text = raw;
    if (result.text.size() > kMaxSceneBytes) return result;

    // Multiple marks are ambiguous and left to the strict parser.
    size_t mark = 0;
    while (result.text.compare(mark, kByteOrderMarkLength, kByteOrderMark) == 0) {
        mark += kByteOrderMarkLength;
    }
    if (mark == kByteOrderMarkLength) {
        result.text.erase(0, mark);
        result.changes.emplace_back("bom_removed");
    }

    if (NormalizeNewlines(&result.text)) result.changes.emplace_back("newlines_normalized");

    std::string inner;
    if (TryUnwrapSingleEnvelope(result.text, &inner)) {
        std::string nested;
        if (!TryUnwrapSingleEnvelope(inner, &nested)) {
            result.text = std::move(inner);
            result.changes.emplace_back("markdown_envelope_removed");
        }
    }

    bool semicolons = false, commas = false;
    std::string rebuilt;
    rebuilt.reserve(result.text.size());
    size_t start = 0;
    for (;;) {
        const size_t newline = result.text.find('\n', start);
        const size_t end = newline == std::string::npos ? result.text.size() : newline;
        std::string line = result.text.substr(start, end - start);
        std::string changed_line;
        if (RemoveTerminalSemicolons(line, &changed_line)) {
            semicolons = true;
            line.swap(changed_line);
        }
        if (RemoveTrailingListCommas(line, &changed_line)) {
            commas = true;
            line.swap(changed_line);
        }
        rebuilt += line;
        if (newline == std::string::npos) break;
        rebuilt += '\n';
        start = newline + 1;
    }
    result.text.swap(rebuilt);
    if (semicolons) result.changes.emplace_back("terminal_semicolon_removed");
    if (commas) result.changes.emplace_back("trailing_list_comma_removed");
    return result;
}

} // namespace liminal
