#pragma once
// Native rendering of the Gemma 4 chat template embedded in the GGUF, for the
// message shapes the game sends (enable_thinking = false, no tools, text only).
// Output parts match llama.cpp's common_chat_session byte for byte, including
// which parts come from message input (tokenized without special tokens).
#include "core/tokenizer.h"

#include <string>
#include <vector>

namespace liminal::engine {

struct ChatMessage {
    std::string role;
    std::string content;
};

struct ChatPart {
    std::string text;
    bool is_input = false;
};

struct ChatPrompt {
    std::vector<ChatPart> parts;
    std::string text;              // concatenated parts (without the BOS text)
    std::vector<int32_t> tokens;   // BOS + tokenized parts
    std::vector<int> message_end;  // token count after each message's end marker
};

std::vector<ChatPart> RenderChatParts(const std::vector<ChatMessage>& messages, std::vector<size_t>* message_end_part = nullptr);
ChatPrompt RenderChat(const Tokenizer& tokenizer, const std::vector<ChatMessage>& messages);

}  // namespace liminal::engine
