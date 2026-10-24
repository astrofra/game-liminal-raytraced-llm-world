#include "llm_channels.h"
#include <stdio.h>

int main()
{
    int errors = 0;
    const auto check = [&errors](bool value, const char* label) {
        if (!value) { fprintf(stderr, "FAIL: %s\n", label); ++errors; }
    };
    const std::string raw = "[THINK]{\"draft\":true} abandoned[/THINK]{\"final\":true}";
    for (size_t length = 0; length <= raw.size(); ++length) {
        const auto result = liminal::InspectMinistralChannels(raw.substr(0, length), "", true, false);
        check(result.error.empty(), "partial thought parsing");
        check(result.final_text.find("draft") == std::string::npos, "draft leaked");
        check(result.final_text.find("THINK") == std::string::npos, "marker leaked");
    }
    const auto final = liminal::InspectMinistralChannels(raw, "", true, true);
    check(final.error.empty() && final.final_text == "{\"final\":true}", "final extraction");
    check(!liminal::InspectMinistralChannels("{\"draft\":true}", "[THINK]", true, true).error.empty(), "unterminated prefill");
    check(liminal::InspectMinistralChannels("draft[/THINK]ok", "[THINK]", true, true).final_text == "ok", "prefix thought");
    check(!liminal::InspectMinistralChannels("[THINK]draft", "", true, true).error.empty(), "unterminated thought");
    check(!liminal::InspectMinistralChannels("[THINK]draft[/THINK]ok", "", false, true).error.empty(), "instruct unexpected thought");
    check(liminal::InspectMinistralChannels("ok", "", true, true).final_text == "ok", "reasoning optional thought");
    const std::string direct = "{\"ok\":true}";
    for (size_t length = 1; length <= direct.size(); ++length) {
        const auto result = liminal::InspectMinistralChannels(direct.substr(0, length), "", true, false);
        check(result.error.empty() && result.final_text == direct.substr(0, length) && !result.has_reasoning,
              "direct final answer on reasoning checkpoint");
    }
    check(!liminal::InspectMinistralChannels("[THINK][THINK]draft[/THINK]ok", "", true, true).error.empty(), "nested thought");
    check(!liminal::InspectMinistralChannels("", "", false, true).error.empty(), "empty completion");
    check(liminal::InspectMinistralChannels("{\"ok\":true}", "", false, true).error.empty(), "instruct JSON");
    printf("Ministral channel checks: %s\n", errors ? "FAILED" : "passed");
    return errors ? 1 : 0;
}
