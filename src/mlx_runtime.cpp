#include "mlx_runtime.h"
#include "llm_channels.h"
#include "resource_paths.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;

namespace liminal {
namespace {
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr size_t kMaxFrame = 4 * 1024 * 1024;
struct Worker {
    pid_t pid = -1;
    int socket = -1;
    std::string pending;
    void stop() {
        if (socket >= 0) { close(socket); socket = -1; }
        if (pid > 0) {
            // Hard cancellation also interrupts Metal prefill/load, with no abandoned worker.
            kill(pid, SIGKILL);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
            pid = -1;
        }
        pending.clear();
    }
    ~Worker() { stop(); }
    void start() {
        if (pid > 0) return;
        const char* override_python = std::getenv("LIMINAL_MLX_PYTHON");
        std::string python = override_python ? override_python : LIMINAL_MLX_PYTHON;
        const auto script = ResourcePath("scripts/mlx_worker.py");
        if (!std::filesystem::is_regular_file(python) || !std::filesystem::is_regular_file(script))
            throw std::runtime_error("MLX runtime missing; run scripts/setup_macos.py first (LIMINAL_MLX_PYTHON may override Python)");
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) throw std::runtime_error("Cannot create MLX channel");
        for (int fd : sockets) fcntl(fd, F_SETFD, FD_CLOEXEC);
        int enabled = 1;
        setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, sockets[0]);
        posix_spawn_file_actions_addclose(&actions, sockets[1]);
        char* argv[] = {const_cast<char*>(python.c_str()), const_cast<char*>("-u"),
                       const_cast<char*>(script.c_str()), nullptr};
        const int error = posix_spawn(&pid, python.c_str(), &actions, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        close(sockets[1]);
        if (error) { close(sockets[0]); pid = -1; throw std::runtime_error(std::string("Cannot start MLX: ") + strerror(error)); }
        socket = sockets[0];
        fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) | O_NONBLOCK);
    }
};
Worker worker;
}

void ShutdownMlxRuntime() { worker.stop(); }

bool GenerateMlxCompletion(const LlmGenerationConfig& config, const std::vector<LlmPromptMessage>& messages,
    LlmStreamCallback callback, void* user, LlmGenerationResult* result, bool count_only) {
    const auto started = Clock::now();
    std::string final;
    const auto fail = [&](const std::string& reason, const std::string& message) {
        worker.stop();
        result->success = false;
        result->stop_reason = reason;
        result->error_message = message;
        result->response_text.clear();
        return false;
    };
    const auto alive = [&]() {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-started).count() >= config.timeout_ms)
            return fail("timeout", "MLX request deadline exceeded");
        if (callback && !callback(final.c_str(), "", user)) return fail("cancelled", "MLX request cancelled");
        return true;
    };
    try {
        if (!alive()) return false;
        worker.start();
        auto input = json::array();
        for (const auto& message : messages) input.push_back({{"role", message.role}, {"content", message.content}});
        const std::string request = json({{"protocol", 1}, {"model_path", config.model_path},
            {"messages", input}, {"count_only", count_only}, {"n_ctx", config.n_ctx},
            {"n_predict", config.n_predict}, {"n_batch", config.n_batch}, {"seed", config.seed},
            {"temperature", config.temperature}, {"top_k", config.top_k}, {"top_p", config.top_p},
            {"use_json_grammar", config.use_json_grammar}, {"json_schema", config.json_schema}}).dump() + "\n";
        if (request.size() > kMaxFrame) return fail("protocol_error", "MLX request exceeds frame limit");
        size_t sent = 0;
        while (sent < request.size()) {
            if (!alive()) return false;
            pollfd descriptor{worker.socket, POLLOUT, 0};
            int ready = poll(&descriptor, 1, 20);
            if (ready < 0 && errno == EINTR) continue;
            if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) return fail("worker_error", "MLX worker disconnected during send");
            if (!ready) continue;
            const auto n = send(worker.socket, request.data()+sent, request.size()-sent, 0);
            if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
            if (n <= 0) return fail("worker_error", "Cannot write MLX request");
            sent += static_cast<size_t>(n);
        }
        for (;;) {
            if (!alive()) return false;
            const auto newline = worker.pending.find('\n');
            if (newline == std::string::npos) {
                if (worker.pending.size() > kMaxFrame) return fail("protocol_error", "MLX response exceeds frame limit");
                pollfd descriptor{worker.socket, POLLIN, 0};
                int ready = poll(&descriptor, 1, 20);
                if (ready < 0 && errno == EINTR) continue;
                if (ready < 0) return fail("worker_error", "MLX channel poll failed");
                if (!ready) continue;
                char buffer[16384];
                const auto n = read(worker.socket, buffer, sizeof(buffer));
                if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
                if (n <= 0) return fail("worker_error", "MLX worker exited; see stderr for dependency/model diagnostics");
                worker.pending.append(buffer, static_cast<size_t>(n));
                continue;
            }
            if (newline > kMaxFrame) return fail("protocol_error", "MLX response exceeds frame limit");
            const auto event = json::parse(worker.pending.substr(0, newline));
            worker.pending.erase(0, newline+1);
            const auto type = event.at("type").get<std::string>();
            if (type == "error") return fail(event.value("stop_reason", "error"), event.value("error_message", "MLX failure"));
            if (type == "ready" || type == "done") {
                result->prompt_text = event.at("prompt_text").get<std::string>();
                result->prompt_tokens = event.at("prompt_tokens").get<int>();
                result->model_reused = event.at("model_reused").get<bool>();
                result->load_time_ms = event.at("load_time_ms").get<double>();
            }
            if (type == "delta") {
                result->raw_text += event.at("text").get<std::string>();
                if (result->raw_text.size() > kMaxFrame) return fail("protocol_error", "MLX output exceeds limit");
                result->generated_tokens = event.at("generated_tokens").get<int>();
                result->final_tokens = result->generated_tokens;
                const auto guarded = GuardFinalChannel(result->raw_text, false);
                if (!guarded.error.empty()) return fail("protocol_error", guarded.error);
                if (guarded.text.compare(0, final.size(), final) != 0) return fail("protocol_error", "MLX final prefix changed");
                const auto delta = guarded.text.substr(final.size());
                final = guarded.text;
                if (callback && !delta.empty() && !callback(final.c_str(), delta.c_str(), user)) return fail("cancelled", "MLX generation cancelled");
            } else if (type == "done") {
                if (!count_only) {
                    if (event.at("response_text").get<std::string>() != result->raw_text) return fail("protocol_error", "MLX final stream mismatch");
                    const auto guarded = GuardFinalChannel(result->raw_text, true);
                    if (!guarded.error.empty()) return fail("protocol_error", guarded.error);
                    if (!event.value("reached_eog", false)) return fail("token_limit", "Missing MLX EOS");
                    if (config.use_json_grammar) {
                        const auto parsed = json::parse(guarded.text, nullptr, false);
                        if (!parsed.is_object()) return fail("invalid_final_json", "MLX final JSON is not an object");
                    }
                    result->response_text = guarded.text;
                    result->reached_eog = true;
                    result->inference_time_ms = event.at("inference_time_ms").get<double>();
                    result->prompt_time_ms = event.at("prompt_time_ms").get<double>();
                    result->generation_time_ms = event.at("generation_time_ms").get<double>();
                }
                if (!alive()) return false;
                result->success = true;
                result->stop_reason = count_only ? "count" : "eos";
                return true;
            } else if (type != "ready") return fail("protocol_error", "Unknown MLX event");
        }
    } catch (const std::exception& error) { return fail("worker_error", error.what()); }
}
}
