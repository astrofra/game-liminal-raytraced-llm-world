#ifndef LIMINAL_LLM_CHANNELS_H
#define LIMINAL_LLM_CHANNELS_H

#include <string>

namespace liminal {

struct LlmChannels {
    std::string thought;
    std::string final_text;
    std::string error;
    bool thinking = false;
    bool has_reasoning = false;
};

// Guard the Ministral protocol independently of the template parser. Incomplete
// markers remain private, and an unfinished thought never becomes final text.
inline LlmChannels InspectMinistralChannels(const std::string& raw,
                                           const std::string& generation_prefix,
                                           bool reasoning, bool complete)
{
    LlmChannels result;
    std::string text = raw;
    const std::string start = "[THINK]";
    const std::string end = "[/THINK]";
    const size_t prefix_start = generation_prefix.rfind(start);
    if (prefix_start != std::string::npos && generation_prefix.find(end, prefix_start) == std::string::npos) {
        text = generation_prefix.substr(prefix_start) + raw;
    }
    const size_t first = text.find_first_not_of(" \r\n\t");
    text = first == std::string::npos ? "" : text.substr(first);
    if (reasoning) {
        if (text.size() < start.size() && start.compare(0, text.size(), text) == 0) {
            result.thinking = true;
            if (complete) result.error = "Incomplete reasoning opening marker";
            return result;
        }
        // The upstream Ministral parser makes the thought block optional.
        // A direct final answer is valid; an opened thought must always close.
        if (text.compare(0, start.size(), start) == 0) {
            result.has_reasoning = true;
            const size_t close = text.find(end, start.size());
            if (close == std::string::npos) {
                result.thought = text.substr(start.size());
                result.thinking = true;
                if (complete) result.error = "Unclosed reasoning channel";
                return result;
            }
            result.thought = text.substr(start.size(), close - start.size());
            text = text.substr(close + end.size());
            if (result.thought.find(start) != std::string::npos) {
                result.error = "Nested reasoning marker";
                return result;
            }
        }
    }
    for (const char* marker : {"[THINK]", "[/THINK]", "<think>", "</think>"}) {
        if (text.find(marker) != std::string::npos) {
            result.error = "Unexpected reasoning marker in final channel";
            return result;
        }
        if (!complete) {
            const std::string tag(marker);
            for (size_t n = 1; n < tag.size() && n <= text.size(); ++n) {
                if (text.compare(text.size() - n, n, tag, 0, n) == 0) {
                    text.resize(text.size() - n);
                    break;
                }
            }
        }
    }
    result.final_text = text;
    if (complete && text.find_first_not_of(" \r\n\t") == std::string::npos) {
        result.error = "Empty final channel";
    }
    return result;
}

} // namespace liminal
#endif
