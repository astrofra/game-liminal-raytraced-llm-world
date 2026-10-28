#include "llm_runtime.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <sys/wait.h>

using namespace liminal;
void require(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
int main() {
    try {
        setenv("LIMINAL_MLX_PYTHON", LIMINAL_TEST_PEER, 1);
        LlmGenerationConfig config;
        config.timeout_ms = 5000;
        config.n_predict = 10;
        LlmGenerationResult result;
        auto call = [&](const char* mode) {
            return GenerateChatCompletion(config, {{"user", mode}}, &result);
        };
        require(call("ok") && result.response_text == "{}", "initial response");
        int tokens = 0; std::string error;
        const auto before = QueryLlmRuntimeTotals().calls;
        require(CountChatPromptTokens(config, {{"user", "count"}}, nullptr, nullptr, &tokens, &error) && tokens == 3,
                "token counter uses worker template");
        require(QueryLlmRuntimeTotals().calls == before, "count is not a generation");
        require(!call("crash") && result.stop_reason == "worker_error", "worker crash is reported");
        require(call("ok"), "restart after crash");
        require(!call("private") && result.stop_reason == "protocol_error" && result.response_text.empty(), "private channel rejected");
        require(call("ok"), "restart after protocol failure");
        config.timeout_ms = 100;
        const auto start = std::chrono::steady_clock::now();
        require(!call("hang") && result.stop_reason == "timeout", "deadline during blocked read");
        require(std::chrono::steady_clock::now()-start < std::chrono::seconds(2), "deadline bounded");
        config.timeout_ms = 5000;
        require(call("ok"), "restart after timeout");
        require(!GenerateChatCompletion(config, {{"user", "hang-after-token"}},
            [](const char* text, const char*, void*) { return !*text; }, nullptr, &result) &&
            result.stop_reason == "cancelled" && result.response_text.empty(), "stream cancellation");
        require(call("ok"), "restart after cancellation");
        config.n_ctx = 1;
        require(!call("ok") && result.stop_reason == "error", "invalid generation bounds");
        ShutdownLlmRuntime();
        int status = 0;
        require(waitpid(-1, &status, WNOHANG) == -1, "all children reaped");
        std::cout << "MLX transport lifecycle checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        ShutdownLlmRuntime();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
