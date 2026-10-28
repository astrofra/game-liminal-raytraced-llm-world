#include "core/chat_format.h"

#include <cctype>

namespace liminal::engine {

namespace {

// jinja `trim` as implemented by llama.cpp (common/jinja/string.cpp): ASCII isspace.
static std::string Trim(const std::string& text)
{
    size_t begin = 0, end = text.size();
    while (begin < end && isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

// Template macro strip_thinking: drop every <|channel>...<channel|> span, then trim.
static std::string StripThinking(const std::string& text)
{
    static const std::string close = "<channel|>";
    static const std::string open = "<|channel>";
    std::string result;
    size_t start = 0;
    while (true) {
        const size_t at = text.find(close, start);
        const std::string part = text.substr(start, at == std::string::npos ? std::string::npos : at - start);
        const size_t channel = part.find(open);
        result += channel == std::string::npos ? part : part.substr(0, channel);
        if (at == std::string::npos) break;
        start = at + close.size();
    }
    return Trim(result);
}

struct PartWriter {
    std::vector<ChatPart>* parts;
    // Consecutive template text merges into one part; empty input disappears.
    void Template(const std::string& text) {
        if (text.empty()) return;
        if (!parts->empty() && !parts->back().is_input) parts->back().text += text;
        else parts->push_back(ChatPart{text, false});
    }
    void Input(const std::string& text) {
        if (!text.empty()) parts->push_back(ChatPart{text, true});
    }
};

}  // namespace

std::vector<ChatPart> RenderChatParts(const std::vector<ChatMessage>& messages, std::vector<size_t>* message_end_part)
{
    std::vector<ChatPart> parts;
    PartWriter out{&parts};
    size_t first = 0;
    if (!messages.empty() && (messages[0].role == "system" || messages[0].role == "developer")) {
        out.Template("<|turn>system\n");
        out.Input(Trim(messages[0].content));
        out.Template("<turn|>\n");
        first = 1;
        if (message_end_part) message_end_part->push_back(parts.size());
    }
    for (size_t i = first; i < messages.size(); ++i) {
        const ChatMessage& message = messages[i];
        if (message.role == "tool") continue;
        const bool model = message.role == "assistant";
        bool continues_model = false;
        for (size_t j = i; j-- > first;) {
            if (messages[j].role == "tool") continue;
            continues_model = model && messages[j].role == "assistant";
            break;
        }
        if (!continues_model) {
            out.Template("<|turn>");
            if (model) out.Template("model");
            else out.Input(message.role);
            out.Template("\n");
        }
        out.Input(model ? StripThinking(message.content) : Trim(message.content));
        out.Template("<turn|>\n");
        if (message_end_part) message_end_part->push_back(parts.size());
    }
    out.Template("<|turn>model\n");
    return parts;
}

ChatPrompt RenderChat(const Tokenizer& tokenizer, const std::vector<ChatMessage>& messages)
{
    ChatPrompt prompt;
    std::vector<size_t> message_end_part;
    prompt.parts = RenderChatParts(messages, &message_end_part);
    bool first = true;
    size_t part_index = 0;
    size_t next_end = 0;
    for (const ChatPart& part : prompt.parts) {
        prompt.text += part.text;
        if (!part.text.empty()) {
            const auto tokens = Tokenize(tokenizer, part.text, first, !part.is_input);
            prompt.tokens.insert(prompt.tokens.end(), tokens.begin(), tokens.end());
            first = false;
        }
        ++part_index;
        // A message boundary lies inside the merged template part that follows it;
        // record the token count once that part is emitted (used for prefix reuse).
        while (next_end < message_end_part.size() && message_end_part[next_end] <= part_index) {
            prompt.message_end.push_back(static_cast<int>(prompt.tokens.size()));
            ++next_end;
        }
    }
    return prompt;
}

}  // namespace liminal::engine
