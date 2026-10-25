#pragma once
#include <string>
namespace liminal {
struct FinalChannel { std::string text; std::string error; };
// Gemma is requested in final-only mode. The upstream parser is authoritative;
// this guard prevents its recovery from admitting private or incomplete tags.
inline FinalChannel GuardFinalChannel(const std::string& text, bool complete) {
    FinalChannel result{text, ""};
    for (const std::string marker : {"<|", "<channel|>", "<tool_call|>", "<turn|>", "</think>", "<think>"}) {
        if (text.find(marker) != std::string::npos) {
            result.text.clear(); result.error = "Unexpected control channel in final-only generation"; return result;
        }
        for (size_t n=1; n<marker.size() && n<=text.size(); ++n) {
            if (text.compare(text.size()-n,n,marker,0,n)==0) {
                if (complete) { result.text.clear(); result.error="Incomplete control channel"; return result; }
                if (result.text.size()>text.size()-n) result.text.resize(text.size()-n);
            }
        }
    }
    if (complete && text.find_first_not_of(" \r\n\t")==std::string::npos) result.error="Empty final channel";
    return result;
}
}
