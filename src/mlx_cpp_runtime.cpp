#include "mlx_cpp_runtime.h"
#include "resource_paths.h"
#include "mlx_cpp_model.h"
#include "mlx_cpp_manifest.generated.h"
#include "llm_channels.h"
#include "llama.h"
#include "common.h"
#include "chat.h"
#include "json-schema-to-grammar.h"
#include "nlohmann/json.hpp"
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace liminal {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
using ModelPtr = std::unique_ptr<llama_model, decltype(&llama_model_free)>;
using SamplerPtr = std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)>;
std::unique_ptr<MlxCppModel> weights;
ModelPtr vocabulary(nullptr, llama_model_free);
std::string resident_path;
using Stamp = std::pair<uintmax_t, std::filesystem::file_time_type>;
std::map<std::string, Stamp> resident_stamps;
const nlohmann::json manifest = nlohmann::json::parse(kMlxCppManifest);

struct Stopped {
    bool cancelled;
};
struct InvalidModel {
    std::string reason, message;
};
std::string read_text(const std::filesystem::path &path) {
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("Cannot read " + path.u8string());
    return {std::istreambuf_iterator<char>(stream), {}};
}
std::string sha256(const std::filesystem::path &path, const std::function<void()> &alive) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw InvalidModel{"model_read_error", "Cannot read " + path.u8string()};
    CC_SHA256_CTX hash;
    CC_SHA256_Init(&hash);
    std::vector<char> buffer(4 * 1024 * 1024);
    while (stream) {
        alive();
        stream.read(buffer.data(), buffer.size());
        CC_SHA256_Update(&hash, buffer.data(), static_cast<CC_LONG>(stream.gcount()));
    }
    if (!stream.eof())
        throw InvalidModel{"model_read_error", "Cannot finish model verification"};
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &hash);
    static const char hex[] = "0123456789abcdef";
    std::string result;
    for (auto byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
bool load(const std::filesystem::path &path, const std::function<void()> &alive) {
    if (!std::filesystem::is_directory(path))
        throw InvalidModel{"model_missing", "Native MLX model directory is missing"};
    std::map<std::string, Stamp> stamps;
    for (auto it = manifest["files"].begin(); it != manifest["files"].end(); ++it) {
        alive();
        const auto file = path / it.key();
        if (!std::filesystem::is_regular_file(file))
            throw InvalidModel{"model_missing", "Missing " + file.u8string()};
        stamps.emplace(it.key(),
                       Stamp{std::filesystem::file_size(file), std::filesystem::last_write_time(file)});
    }
    const auto normalized = std::filesystem::absolute(path).lexically_normal().u8string();
    if (weights && vocabulary && resident_path == normalized && resident_stamps == stamps)
        return true;
    weights.reset();
    vocabulary.reset();
    resident_stamps.clear();
    resident_path.clear();
    for (auto it = manifest["files"].begin(); it != manifest["files"].end(); ++it) {
        if (stamps.at(it.key()).first != it.value()["size_bytes"].get<uintmax_t>() ||
            sha256(path / it.key(), alive) != it.value()["sha256"].get<std::string>())
            throw InvalidModel{"model_corrupt", "Native MLX manifest mismatch: " + it.key()};
    }
    alive();
    auto params = llama_model_default_params();
    params.vocab_only = true;
    params.n_gpu_layers = 0;
    vocabulary.reset(llama_model_load_from_file((path / "tokenizer.gguf").c_str(), params));
    if (!vocabulary || llama_vocab_n_tokens(llama_model_get_vocab(vocabulary.get())) != 262144)
        throw InvalidModel{"model_load_error", "Invalid Gemma vocabulary"};
    alive();
    weights =
        std::make_unique<MlxCppModel>((path / "model.safetensors").u8string(), ResourcePath("mlx.metallib"));
    alive();
    resident_path = normalized;
    resident_stamps = std::move(stamps);
    return false;
}

// Same rejection-first strategy as common_sampler, operating on native MLX logits.
llama_token sample(const std::vector<float> &logits, llama_sampler *chain, llama_sampler *grammar) {
    std::vector<llama_token_data> data;
    data.reserve(logits.size());
    for (size_t i = 0; i < logits.size(); ++i)
        data.push_back({static_cast<llama_token>(i), logits[i], 0});
    llama_token_data_array candidates{data.data(), data.size(), -1, false};
    llama_sampler_apply(chain, &candidates);
    if (candidates.selected < 0)
        throw std::runtime_error("Sampler selected no token");
    auto chosen = candidates.data[candidates.selected];
    if (grammar) {
        llama_token_data_array single{&chosen, 1, -1, false};
        llama_sampler_apply(grammar, &single);
        if (!std::isfinite(chosen.logit)) {
            for (size_t i = 0; i < logits.size(); ++i)
                data[i] = {static_cast<llama_token>(i), logits[i], 0};
            candidates = {data.data(), data.size(), -1, false};
            llama_sampler_apply(grammar, &candidates);
            llama_sampler_apply(chain, &candidates);
            if (candidates.selected < 0 || !std::isfinite(candidates.data[candidates.selected].logit))
                throw std::runtime_error("Grammar allows no finite token");
            chosen = candidates.data[candidates.selected];
        }
        llama_sampler_accept(grammar, chosen.id);
    }
    llama_sampler_accept(chain, chosen.id);
    return chosen.id;
}
} // namespace

void ShutdownMlxCppRuntime() {
    weights.reset();
    vocabulary.reset();
    resident_path.clear();
    resident_stamps.clear();
}

bool GenerateMlxCppCompletion(const LlmGenerationConfig &config,
                              const std::vector<LlmPromptMessage> &messages, LlmStreamCallback callback,
                              void *userdata, LlmGenerationResult *result, bool count_only) {
    const auto started = Clock::now();
    auto fail = [&](const std::string &reason, const std::string &message) {
        result->stop_reason = reason;
        result->error_message = message;
        result->response_text.clear();
        return false;
    };
    auto alive = [&]() {
        if (elapsed(started) >= config.timeout_ms)
            throw Stopped{false};
        if (callback && !callback("", "", userdata))
            throw Stopped{true};
    };
    try {
        alive();
        const auto load_started = Clock::now();
        result->model_reused = load(std::filesystem::u8path(config.model_path), alive);
        result->load_time_ms = elapsed(load_started);
        result->offloaded_layers = 42;
        result->total_layers = 42;
        if (result->model_reused)
            result->load_time_ms = 0;
        weights->reset();
        struct ClearSession {
            ~ClearSession() {
                if (weights)
                    weights->reset();
            }
        } clear_session;
        const auto *vocab = llama_model_get_vocab(vocabulary.get());
        auto templates = common_chat_templates_init(
            vocabulary.get(), read_text(std::filesystem::u8path(config.model_path) / "chat_template.jinja"));
        common_chat_templates_inputs inputs;
        inputs.use_jinja = true;
        inputs.enable_thinking = false;
        inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        for (const auto &source : messages) {
            common_chat_msg msg;
            msg.role = source.role;
            msg.content = source.content;
            inputs.messages.push_back(msg);
        }
        if (config.use_json_grammar) {
            const std::string instruction =
                "Return only JSON conforming to this schema, including all bounds:\n" +
                (config.json_schema.empty() ? "{\"type\":\"object\"}" : config.json_schema);
            if (!inputs.messages.empty() && inputs.messages.front().role == "system")
                inputs.messages.front().content += "\n" + instruction;
            else {
                common_chat_msg msg;
                msg.role = "system";
                msg.content = instruction;
                inputs.messages.insert(inputs.messages.begin(), msg);
            }
        }
        common_chat_session chat(templates.get(), vocab, inputs);
        result->prompt_text = chat.prompt().str();
        std::vector<int> tokens;
        bool first = true;
        for (const auto &part : chat.prompt().parts) {
            if (part.text.empty())
                continue;
            auto ids = common_tokenize(vocab, part.text, first, !part.is_input);
            tokens.insert(tokens.end(), ids.begin(), ids.end());
            first = false;
        }
        result->prompt_tokens = static_cast<int>(tokens.size());
        if (count_only) {
            result->success = true;
            return true;
        }
        if (tokens.empty() || tokens.size() + config.n_predict + 1 > static_cast<size_t>(config.n_ctx))
            return fail("context_limit", "Prompt and reserved output exceed fixed context budget");
        SamplerPtr grammar(nullptr, llama_sampler_free);
        if (config.use_json_grammar) {
            auto schema =
                common_json::parse(config.json_schema.empty() ? "{\"type\":\"object\"}" : config.json_schema);
            auto source = json_schema_to_grammar(schema);
            grammar.reset(llama_sampler_init_grammar(vocab, source.c_str(), "root"));
            if (!grammar)
                return fail("error", "Cannot initialize JSON grammar");
        }
        SamplerPtr chain(llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free);
        if (config.temperature == 0)
            llama_sampler_chain_add(chain.get(), llama_sampler_init_greedy());
        else {
            llama_sampler_chain_add(chain.get(), llama_sampler_init_top_k(config.top_k));
            llama_sampler_chain_add(chain.get(), llama_sampler_init_top_p(config.top_p, 1));
            llama_sampler_chain_add(chain.get(), llama_sampler_init_temp(config.temperature));
            llama_sampler_chain_add(chain.get(), llama_sampler_init_dist(config.seed));
        }
        const auto inference_started = Clock::now();
        struct Timer {
            LlmGenerationResult *r;
            Clock::time_point t;
            ~Timer() {
                r->inference_time_ms = elapsed(t);
            }
        } timer{result, inference_started};
        for (size_t pos = 0; pos + 1 < tokens.size(); pos += config.n_batch) {
            alive();
            const size_t end = std::min(pos + config.n_batch, tokens.size() - 1);
            weights->step({tokens.begin() + pos, tokens.begin() + end}, false);
        }
        alive();
        auto logits = weights->step({tokens.back()}, true);
        result->prompt_time_ms = elapsed(inference_started);
        const auto generation_started = Clock::now();
        std::string streamed;
        for (int generated = 0; generated <= config.n_predict; ++generated) {
            if (elapsed(started) >= config.timeout_ms)
                throw Stopped{false};
            if (callback && !callback(streamed.c_str(), "", userdata))
                throw Stopped{true};
            const auto token = sample(logits, chain.get(), grammar.get());
            if (llama_vocab_is_eog(vocab, token)) {
                result->reached_eog = true;
                break;
            }
            if (generated == config.n_predict)
                return fail("token_limit", "Total generation budget exhausted");
            const auto piece = common_token_to_piece(vocab, token, true);
            result->raw_text += piece;
            ++result->generated_tokens;
            ++result->final_tokens;
            const auto guard = GuardFinalChannel(result->raw_text, false);
            if (!guard.error.empty())
                return fail("protocol_error", guard.error);
            common_chat_input chunk;
            chunk.append(piece, token);
            const auto &parsed = chat.feed(chunk);
            if (!parsed.reasoning_content.empty() || !parsed.tool_calls.empty())
                return fail("protocol_error", "Unexpected private or tool output");
            auto channels = GuardFinalChannel(parsed.content, false);
            if (!channels.error.empty() || channels.text.compare(0, streamed.size(), streamed) != 0)
                return fail("protocol_error", "Invalid final stream");
            const auto delta = channels.text.substr(streamed.size());
            streamed = channels.text;
            if (callback && !delta.empty() && !callback(streamed.c_str(), delta.c_str(), userdata))
                throw Stopped{true};
            logits = weights->step({token}, true);
        }
        result->generation_time_ms = elapsed(generation_started);
        if (!result->reached_eog)
            return fail("token_limit", "Missing end of generation");
        const auto guard = GuardFinalChannel(result->raw_text, true);
        if (!guard.error.empty())
            return fail("protocol_error", guard.error);
        const auto &parsed = chat.finish();
        if (!parsed.reasoning_content.empty() || !parsed.tool_calls.empty())
            return fail("protocol_error", "Unexpected private or tool output");
        auto channels = GuardFinalChannel(parsed.content, true);
        if (!channels.error.empty())
            return fail("protocol_error", channels.error);
        if (config.use_json_grammar) {
            auto value = nlohmann::json::parse(channels.text, nullptr, false);
            if (value.is_discarded() || !value.is_object())
                return fail("invalid_final_json", "Final channel is not an object");
        }
        if (elapsed(started) >= config.timeout_ms)
            throw Stopped{false};
        if (callback && !callback(streamed.c_str(), "", userdata))
            throw Stopped{true};
        result->response_text = channels.text;
        result->success = true;
        result->stop_reason = "eos";
        return true;
    } catch (const Stopped &stopped) {
        return fail(stopped.cancelled ? "cancelled" : "timeout", "Native MLX request stopped");
    } catch (const InvalidModel &invalid) {
        ShutdownMlxCppRuntime();
        return fail(invalid.reason, invalid.message);
    } catch (const std::exception &error) {
        return fail("error", error.what());
    }
}
} // namespace liminal
