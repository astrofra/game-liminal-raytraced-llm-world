#include "llm_runtime.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <string.h>

#if defined(LIMINAL_HAVE_LLAMA_CPP)
#include "llama.h"
#endif

namespace liminal {

namespace {

#if !defined(LIMINAL_LLAMA_VENDORED_COMMIT)
#define LIMINAL_LLAMA_VENDORED_COMMIT "unknown"
#endif

static bool g_llama_backend_initialized = false;

static bool ReadTextFile(const char* path, std::string* text)
{
    if (!path || !text) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    text->clear();
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        text->append(buffer);
    }

    fclose(file);
    return true;
}

#if defined(LIMINAL_HAVE_LLAMA_CPP)
static bool ApplyChatTemplate(
    const llama_model* model,
    const std::vector<LlmPromptMessage>& messages,
    std::string* prompt_text,
    std::string* error_message)
{
    if (!model || !prompt_text) {
        if (error_message) {
            *error_message = "Invalid model or prompt buffer.";
        }
        return false;
    }

    std::vector<llama_chat_message> chat;
    size_t reserve_size = 0;
    chat.reserve(messages.size());

    for (size_t index = 0; index < messages.size(); ++index) {
        const LlmPromptMessage& message = messages[index];
        chat.push_back(llama_chat_message{message.role.c_str(), message.content.c_str()});
        reserve_size += message.role.size() + message.content.size();
    }

    const char* template_text = llama_model_chat_template(model, 0);
    if (!template_text || !template_text[0]) {
        prompt_text->clear();
        for (size_t index = 0; index < messages.size(); ++index) {
            prompt_text->append(messages[index].role);
            prompt_text->append(":\n");
            prompt_text->append(messages[index].content);
            prompt_text->append("\n\n");
        }
        prompt_text->append("assistant:\n");
        return true;
    }

    std::vector<char> buffer(std::max<size_t>(2048, reserve_size * 2 + 256));
    int32_t result = llama_chat_apply_template(template_text, chat.data(), chat.size(), true, buffer.data(), static_cast<int32_t>(buffer.size()));
    if (result < 0) {
        if (error_message) {
            *error_message = "Chat template application failed.";
        }
        return false;
    }

    if (static_cast<size_t>(result) > buffer.size()) {
        buffer.resize(result);
        result = llama_chat_apply_template(template_text, chat.data(), chat.size(), true, buffer.data(), static_cast<int32_t>(buffer.size()));
    }

    if (result < 0 || static_cast<size_t>(result) > buffer.size()) {
        if (error_message) {
            *error_message = "Chat template application produced an invalid size.";
        }
        return false;
    }

    prompt_text->assign(buffer.data(), result);
    return true;
}

static bool TokenizePrompt(
    const llama_vocab* vocab,
    const std::string& prompt_text,
    std::vector<llama_token>* tokens,
    std::string* error_message)
{
    if (!vocab || !tokens) {
        if (error_message) {
            *error_message = "Invalid tokenizer inputs.";
        }
        return false;
    }

    const int32_t required = llama_tokenize(
        vocab,
        prompt_text.c_str(),
        static_cast<int32_t>(prompt_text.size()),
        0,
        0,
        true,
        true);
    if (required >= 0) {
        if (error_message) {
            *error_message = "Unexpected tokenizer size response.";
        }
        return false;
    }

    tokens->resize(static_cast<size_t>(-required));
    const int32_t written = llama_tokenize(
        vocab,
        prompt_text.c_str(),
        static_cast<int32_t>(prompt_text.size()),
        tokens->data(),
        static_cast<int32_t>(tokens->size()),
        true,
        true);
    if (written < 0) {
        if (error_message) {
            *error_message = "Tokenizer failed while writing tokens.";
        }
        return false;
    }

    tokens->resize(static_cast<size_t>(written));
    return true;
}

static bool AppendTokenPiece(const llama_vocab* vocab, llama_token token, std::string* output_text, std::string* error_message)
{
    if (!vocab || !output_text) {
        if (error_message) {
            *error_message = "Invalid detokenization inputs.";
        }
        return false;
    }

    char local_buffer[256];
    int32_t piece_size = llama_token_to_piece(vocab, token, local_buffer, static_cast<int32_t>(sizeof(local_buffer)), 0, true);
    if (piece_size < 0) {
        std::vector<char> dynamic_buffer(static_cast<size_t>(-piece_size));
        piece_size = llama_token_to_piece(vocab, token, dynamic_buffer.data(), static_cast<int32_t>(dynamic_buffer.size()), 0, true);
        if (piece_size < 0) {
            if (error_message) {
                *error_message = "Detokenization failed.";
            }
            return false;
        }
        output_text->append(dynamic_buffer.data(), piece_size);
        return true;
    }

    output_text->append(local_buffer, piece_size);
    return true;
}

static int ResolveThreadCount(int configured_threads)
{
    if (configured_threads > 0) {
        return configured_threads;
    }

    const unsigned int concurrency = std::thread::hardware_concurrency();
    return concurrency > 0 ? static_cast<int>(concurrency) : 4;
}
#endif

}  // namespace

bool InitializeLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!g_llama_backend_initialized) {
        llama_log_set([](ggml_log_level, const char*, void*) {}, 0);
        llama_backend_init();
        g_llama_backend_initialized = true;
    }
    return true;
#else
    return false;
#endif
}

void ShutdownLlmRuntime()
{
#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (g_llama_backend_initialized) {
        llama_backend_free();
        g_llama_backend_initialized = false;
    }
#endif
}

bool QueryLlmRuntimeInfo(LlmRuntimeInfo* info)
{
    if (!info) {
        return false;
    }

    info->compiled_with_llama = false;
    info->gpu_offload_supported = false;
    info->vendored_commit = LIMINAL_LLAMA_VENDORED_COMMIT;
    info->system_info.clear();

#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (!InitializeLlmRuntime()) {
        return false;
    }

    info->compiled_with_llama = true;
    info->gpu_offload_supported = llama_supports_gpu_offload();

    const char* system_info = llama_print_system_info();
    if (system_info && system_info[0]) {
        info->system_info = system_info;
    }

    return true;
#else
    info->system_info = "llama.cpp support is not compiled into this binary.";
    return false;
#endif
}

bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmStreamCallback stream_callback,
    void* stream_user_data,
    LlmGenerationResult* result)
{
    if (!result) {
        return false;
    }

    *result = LlmGenerationResult();

#if defined(LIMINAL_HAVE_LLAMA_CPP)
    if (messages.empty()) {
        result->error_message = "No messages were provided for generation.";
        return false;
    }
    if (!InitializeLlmRuntime()) {
        result->error_message = "Failed to initialize llama.cpp runtime.";
        return false;
    }

    std::string prompt_text;
    std::string error_message;

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config.n_gpu_layers;

    std::unique_ptr<llama_model, void (*)(llama_model*)> model(
        llama_model_load_from_file(config.model_path.c_str(), model_params),
        llama_model_free);
    if (!model) {
        result->error_message = "Failed to load model from file: " + config.model_path;
        return false;
    }

    if (!ApplyChatTemplate(model.get(), messages, &prompt_text, &error_message)) {
        result->error_message = error_message;
        return false;
    }
    result->prompt_text = prompt_text;

    const llama_vocab* vocab = llama_model_get_vocab(model.get());
    std::vector<llama_token> prompt_tokens;
    if (!TokenizePrompt(vocab, prompt_text, &prompt_tokens, &error_message)) {
        result->error_message = error_message;
        return false;
    }
    result->prompt_tokens = static_cast<int>(prompt_tokens.size());

    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = static_cast<uint32_t>(std::max(config.n_ctx, result->prompt_tokens + config.n_predict + 32));
    context_params.n_batch = static_cast<uint32_t>(std::max(config.n_batch, result->prompt_tokens));
    context_params.n_ubatch = context_params.n_batch;
    context_params.n_seq_max = 1;
    context_params.n_threads = ResolveThreadCount(config.n_threads);
    context_params.n_threads_batch = ResolveThreadCount(config.n_threads_batch);
    context_params.flash_attn_type = config.flash_attention ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
    context_params.offload_kqv = true;

    std::unique_ptr<llama_context, void (*)(llama_context*)> context(
        llama_init_from_model(model.get(), context_params),
        llama_free);
    if (!context) {
        result->error_message = "Failed to create llama.cpp context.";
        return false;
    }

    llama_set_n_threads(context.get(), context_params.n_threads, context_params.n_threads_batch);

    std::unique_ptr<llama_sampler, void (*)(llama_sampler*)> sampler(
        llama_sampler_chain_init(llama_sampler_chain_default_params()),
        llama_sampler_free);
    if (!sampler) {
        result->error_message = "Failed to create sampler chain.";
        return false;
    }

    std::string grammar_text;
    if (config.use_json_grammar && ReadTextFile(config.grammar_path.c_str(), &grammar_text) && !grammar_text.empty()) {
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_grammar(vocab, grammar_text.c_str(), "root"));
    }

    if (config.temperature <= 0.0f) {
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(40));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(0.95f, 1));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(config.temperature));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(config.seed));
    }

    const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    if (llama_model_has_decoder(model.get())) {
        const int32_t decode_result = llama_decode(
            context.get(),
            llama_batch_get_one(prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size())));
        if (decode_result != 0) {
            result->error_message = "Prompt decode failed.";
            return false;
        }
    } else {
        result->error_message = "The selected model has no decoder.";
        return false;
    }

    std::string response_text;
    for (int generated = 0; generated < config.n_predict; ++generated) {
        const llama_token token = llama_sampler_sample(sampler.get(), context.get(), -1);
        llama_sampler_accept(sampler.get(), token);

        if (llama_vocab_is_eog(vocab, token)) {
            result->reached_eog = true;
            break;
        }

        const size_t previous_size = response_text.size();
        if (!AppendTokenPiece(vocab, token, &response_text, &error_message)) {
            result->error_message = error_message;
            return false;
        }

        const std::string delta_text = response_text.substr(previous_size);
        if (stream_callback && !stream_callback(response_text.c_str(), delta_text.c_str(), stream_user_data)) {
            result->error_message = "Generation aborted by stream callback.";
            result->response_text = response_text;
            return false;
        }

        llama_token next_token = token;
        const int32_t decode_result = llama_decode(context.get(), llama_batch_get_one(&next_token, 1));
        if (decode_result != 0) {
            result->error_message = "Token decode failed during generation.";
            return false;
        }

        ++result->generated_tokens;
    }

    const std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
    result->inference_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    result->response_text = response_text;
    result->success = true;
    return true;
#else
    (void) config;
    (void) messages;
    (void) stream_callback;
    (void) stream_user_data;
    result->error_message = "llama.cpp support is not compiled into this binary.";
    return false;
#endif
}

bool GenerateChatCompletion(
    const LlmGenerationConfig& config,
    const std::vector<LlmPromptMessage>& messages,
    LlmGenerationResult* result)
{
    return GenerateChatCompletion(config, messages, 0, 0, result);
}

void PrintLlmRuntimeInfo(FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    LlmRuntimeInfo info;
    QueryLlmRuntimeInfo(&info);

    fprintf(out, "llama.cpp compiled in: %s\n", info.compiled_with_llama ? "yes" : "no");
    fprintf(out, "llama.cpp vendored commit: %s\n", info.vendored_commit ? info.vendored_commit : "unknown");
    fprintf(out, "GPU offload available: %s\n", info.gpu_offload_supported ? "yes" : "no");

    if (!info.system_info.empty()) {
        fprintf(out, "System info: %s\n", info.system_info.c_str());
    }
}

}  // namespace liminal
