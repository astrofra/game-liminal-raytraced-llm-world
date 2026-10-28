// Gemma 4 E4B graph adapted from mlx-lm 0.32.2 gemma4_text.py.
// Copyright © 2025 Apple Inc. MIT: packaging/MLX-LM-LICENSE.txt.
// Only the pinned dense E4B profile is supported (42 layers, 24 independent KV).
#include "mlx_cpp_model.h"
#include <filesystem>
#include <mlx/mlx.h>
#include <optional>
#include <unordered_map>
#include <cmath>
using namespace mlx::core;
namespace liminal {
struct Cache {
    std::optional<array> k, v;
};
struct MlxCppModel::Impl {
    std::unordered_map<std::string, array> w;
    std::vector<Cache> cache = std::vector<Cache>(24);
    int offset = 0;
    array freqs = array(0.f);
    std::function<std::vector<array>(const std::vector<array> &)> gelu, geglu, cap;
    explicit Impl(const std::string &path) {
        auto tensors = load_safetensors(path).first;
        const std::string prefix = "language_model.model.";
        for (auto &[k, v] : tensors)
            if (k.rfind(prefix, 0) == 0)
                w.emplace(k.substr(prefix.size()), v);
        tensors.clear();
        std::vector<array> weights;
        for (auto &[k, v] : w)
            weights.push_back(v);
        eval(weights);
        freqs = concatenate(
            {power(array(1e6f), arange(0, 128, 2, float32) / 512.f), full({192}, INFINITY, float32)}, 0);
        gelu = compile(
            [](const std::vector<array> &v) {
                auto x = v[0];
                return std::vector<array>{
                    array(0.5f, x.dtype()) * x *
                    (array(1.f, x.dtype()) +
                     tanh(array(float(std::sqrt(2. / M_PI)), x.dtype()) *
                          (x + array(0.044715f, x.dtype()) * power(x, array(3, x.dtype())))))};
            },
            true);
        geglu = compile(
            [](const std::vector<array> &v) {
                auto x = v[0];
                return std::vector<array>{
                    array(0.5f, x.dtype()) * x *
                    (array(1.f, x.dtype()) +
                     tanh(array(float(std::sqrt(2. / M_PI)), x.dtype()) *
                          (x + array(0.044715f, x.dtype()) * power(x, array(3, x.dtype()))))) *
                    v[1]};
            },
            true);
        cap = compile(
            [](const std::vector<array> &v) {
                return std::vector<array>{tanh(v[0] / array(30.f, v[0].dtype())) * array(30.f, v[0].dtype())};
            },
            true);
    }
    array linear(const std::string &p, const array &x) {
        return quantized_matmul(x, w.at(p + ".weight"), w.at(p + ".scales"), w.at(p + ".biases"), true, 64,
                                4);
    }
    array embedding(const std::string &p, const array &ids) {
        return dequantize(take(w.at(p + ".weight"), ids, 0), take(w.at(p + ".scales"), ids, 0),
                          take(w.at(p + ".biases"), ids, 0), 64, 4);
    }
    array norm(const std::string &p, const array &x) {
        return fast::rms_norm(x, w.at(p + ".weight"), 1e-6f);
    }
    void reset() {
        cache = std::vector<Cache>(24);
        offset = 0;
        clear_cache();
    }
    // Scalars use the tensor dtype: C++ float literals would otherwise promote
    // bfloat16 activations to float32, changing both logits and GPU throughput.
    array forward(const std::vector<int> &tokens) {
        int L = tokens.size();
        array ids(tokens.begin(), {1, L}, int32);
        array h = embedding("embed_tokens", ids) * array(float(std::sqrt(2560.f)), bfloat16);
        array per =
            reshape(embedding("embed_tokens_per_layer", ids) * array(16.f, bfloat16), {1, L, 42, 256});
        array projected =
            reshape(linear("per_layer_model_projection", h) * array(float(1. / std::sqrt(2560.)), bfloat16),
                    {1, L, 42, 256});
        per = (norm("per_layer_projection_norm", projected) + per) * array(float(std::sqrt(0.5f)), bfloat16);
        for (int i = 0; i < 42; ++i) {
            bool sliding = i % 6 != 5;
            int D = sliding ? 256 : 512;
            std::string p = "layers." + std::to_string(i), a = p + ".self_attn";
            auto x = norm(p + ".input_layernorm", h);
            auto q =
                transpose(norm(a + ".q_norm", reshape(linear(a + ".q_proj", x), {1, L, 8, D})), {0, 2, 1, 3});
            auto rope = [&](array v) {
                return fast::rope(v, D, false, sliding ? std::optional<float>(10000.f) : std::nullopt, 1.f,
                                  offset, sliding ? std::nullopt : std::optional<array>(freqs));
            };
            q = rope(q);
            int c = i < 24 ? i : (sliding ? 22 : 23);
            if (i < 24) {
                auto k = rope(transpose(norm(a + ".k_norm", reshape(linear(a + ".k_proj", x), {1, L, 2, D})),
                                        {0, 2, 1, 3}));
                auto v = transpose(
                    fast::rms_norm(reshape(linear(a + ".v_proj", x), {1, L, 2, D}), std::nullopt, 1e-6f),
                    {0, 2, 1, 3});
                // Keep recent entries before appending; all queries in the chunk see their own window.
                if (cache[c].k) {
                    auto oldk = *cache[c].k, oldv = *cache[c].v;
                    int N = oldk.shape(2);
                    if (sliding && N > 511) {
                        oldk = slice(oldk, {0, 0, N - 511, 0}, {1, 2, N, D});
                        oldv = slice(oldv, {0, 0, N - 511, 0}, {1, 2, N, D});
                    }
                    k = concatenate({oldk, k}, 2);
                    v = concatenate({oldv, v}, 2);
                }
                cache[c].k = k;
                cache[c].v = v;
            }
            auto k = *cache[c].k, v = *cache[c].v;
            int K = k.shape(2);
            auto qi = reshape(arange(offset, offset + L, int32), {L, 1});
            auto ki = reshape(arange(offset + L - K, offset + L, int32), {1, K});
            auto mask = less_equal(ki, qi);
            if (sliding)
                mask = logical_and(mask, less(qi - ki, array(512)));
            auto att = L == 1 ? fast::scaled_dot_product_attention(q, k, v, 1.f)
                              : ((!sliding || K <= 512)
                                     ? fast::scaled_dot_product_attention(q, k, v, 1.f, "causal")
                                     : fast::scaled_dot_product_attention(q, k, v, 1.f, "", mask));
            h = h + norm(p + ".post_attention_layernorm",
                         linear(a + ".o_proj", reshape(transpose(att, {0, 2, 1, 3}), {1, L, 8 * D})));
            auto ff = norm(p + ".pre_feedforward_layernorm", h);
            auto m = geglu({linear(p + ".mlp.gate_proj", ff), linear(p + ".mlp.up_proj", ff)})[0];
            h = h + norm(p + ".post_feedforward_layernorm", linear(p + ".mlp.down_proj", m));
            auto pi = reshape(slice(per, {0, 0, i, 0}, {1, L, i + 1, 256}), {1, L, 256});
            auto gate = gelu({linear(p + ".per_layer_input_gate", h)})[0] * pi;
            h = (h + norm(p + ".post_per_layer_input_norm", linear(p + ".per_layer_projection", gate))) *
                w.at(p + ".layer_scalar");
        }
        offset += L;
        h = norm("norm", slice(h, {0, L - 1, 0}, {1, L, 2560}));
        return reshape(cap({linear("embed_tokens", h)})[0], {262144});
    }
};

MlxCppModel::MlxCppModel(const std::string &weights, const std::string &metallib) {
    // Configure the resource path before Impl's scalar arrays initialize Metal.
    if (std::filesystem::is_regular_file(metallib))
        metal::set_metallib_path(metallib);
    set_default_device(Device::gpu);
    set_cache_limit(256 * 1024 * 1024);
    impl_ = std::make_unique<Impl>(weights);
}
MlxCppModel::~MlxCppModel() = default;
void MlxCppModel::reset() {
    impl_->reset();
}
std::vector<float> MlxCppModel::step(const std::vector<int> &tokens, bool want_logits) {
    if (tokens.empty())
        throw std::invalid_argument("Empty native MLX input");
    auto logits = impl_->forward(tokens);
    std::vector<array> state;
    for (auto &cache : impl_->cache) {
        state.push_back(*cache.k);
        state.push_back(*cache.v);
    }
    if (want_logits) {
        logits = astype(logits, float32);
        state.push_back(logits);
    }
    eval(state);
    // Gemma's last 18 layers share earlier KV: prefill must evaluate KV state
    // only. Evaluating unused logits would needlessly execute those 18 layers.
    if (!want_logits) {
        clear_cache();
        return {};
    }
    const float *data = logits.data<float>();
    return {data, data + logits.size()};
}
} // namespace liminal
