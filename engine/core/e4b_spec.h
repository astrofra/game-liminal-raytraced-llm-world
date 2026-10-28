#pragma once
// Gemma 4 E4B as stored in the game's llama.cpp GGUF (gemma-4-E4B-it-Q4_K_M).
// The engine runs exactly this model: binding asserts every hyper-parameter and
// tensor shape, so another checkpoint is rejected instead of misinterpreted.
//
// Forward pass (reference: vendor/llama.cpp/src/models/gemma4.cpp at 620d2225):
//   x   = deq(token_embd[t]) * sqrt(2560)
//   pt  = deq(per_layer_token_embd[t]) as [42][256] * sqrt(256)
//   pp  = (per_layer_model_proj . x) / sqrt(2560) as [42][256]
//   ple[l] = (rms(pp[l]) * per_layer_proj_norm + pt[l]) / sqrt(2)
//   for each layer l (hd = 256 sliding / 512 full):
//     h  = rms(x) * attn_norm
//     q  = rope(rms_head(Wq h) * q_norm)                          8 heads
//     if l < 24: k = rope(rms_head(Wk h) * k_norm), v = rms_head(Wv h); cache at pos
//     o  = softmax(q . K[src]^T) . V[src]      scale 1, causal, sliding: pos_q - pos_k < 512
//     x1 = x + rms(Wo o) * post_attention_norm
//     x2 = x1 + rms(Wdown(gelu_tanh(Wgate f) * Wup f)) * post_ffw_norm,  f = rms(x1) * ffn_norm
//     x3 = x2 + rms(Wproj(gelu_tanh(Winp_gate x2) * ple[l])) * post_norm
//     x  = x3 * layer_output_scale
//   logits = 30 * tanh((token_embd . (rms(x) * output_norm)) / 30)
// RoPE is NEOX (pairs i, i + d/2): sliding layers d=256 base 1e4; full layers d=512
// base 1e6 with rope_freqs factors (theta_i = pos * base^(-2i/d) / factor_i).
// Layers 24..41 own no KV and read layer 22 (sliding) or 23 (full).
#include "core/gguf.h"

#include <cstdint>
#include <string>
#include <vector>

namespace liminal::engine {

constexpr int kE4bLayers = 42;
constexpr int kE4bEmbd = 2560;
constexpr int kE4bFfn = 10240;
constexpr int kE4bHeads = 8;
constexpr int kE4bKvHeads = 2;
constexpr int kE4bHeadDimSwa = 256;
constexpr int kE4bHeadDimFull = 512;
constexpr int kE4bPleDim = 256;
constexpr int kE4bVocab = 262144;
constexpr int kE4bSlidingWindow = 512;
constexpr int kE4bKvLayers = 24;  // layers 0..23 own their K/V
constexpr float kE4bRmsEps = 1e-6f;
constexpr float kE4bLogitSoftcap = 30.0f;
constexpr float kE4bRopeBaseFull = 1000000.0f;
constexpr float kE4bRopeBaseSwa = 10000.0f;

// A 2-D weight view into the mapped file: rows of `cols` elements, row-major.
struct WeightRef {
    uint32_t type = 0;
    int64_t cols = 0;  // ne[0], contiguous
    int64_t rows = 1;  // ne[1]
    size_t row_bytes = 0;
    const uint8_t* data = nullptr;
};

struct E4bLayer {
    bool swa = false;
    int head_dim = 0;
    int q_dim = 0;    // 8 * head_dim
    int kv_dim = 0;   // 2 * head_dim
    float rope_base = 0.0f;
    bool owns_kv = false;
    int kv_source = 0;  // layer whose cache this layer attends
    float output_scale = 1.0f;
    WeightRef attn_norm, wq, wk, wv, q_norm, k_norm, wo, post_attention_norm;
    WeightRef ffn_norm, w_gate, w_up, w_down, post_ffw_norm;
    WeightRef inp_gate, proj, post_norm;
};

struct E4bModel {
    WeightRef token_embd;            // Q6_K [2560 x 262144], also the tied LM head
    WeightRef per_layer_token_embd;  // Q6_K [10752 x 262144]
    WeightRef per_layer_model_proj;  // BF16 [2560 x 10752]
    WeightRef per_layer_proj_norm;   // F32 [256]
    WeightRef output_norm;           // F32 [2560]
    std::vector<float> rope_freq_factors;  // 256 factors for full-attention layers
    E4bLayer layers[kE4bLayers];
};

// Validates metadata and tensors against the E4B constants and binds the views.
bool E4bBind(const GgufFile& file, E4bModel* model, std::string* error);

}  // namespace liminal::engine
