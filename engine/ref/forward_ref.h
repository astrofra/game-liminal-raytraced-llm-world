#pragma once
// Scalar f32 CPU implementation of the E4B forward pass (engine/core/e4b_spec.h).
// It is the oracle the GPU backends are measured against: literal, slow, f32 KV.
#include "core/e4b_spec.h"

#include <map>
#include <string>
#include <vector>

namespace liminal::engine {

struct RefState {
    int n_past = 0;
    std::vector<float> k[kE4bKvLayers];  // [position][kv_dim]
    std::vector<float> v[kE4bKvLayers];
};

// Optional capture of last-position hidden states, named like llama.cpp's graph
// callbacks: inp_scaled, attn_post_norm-<l>, ffn_post_norm-<l>,
// per_layer_embd_out-<l>, l_out-<l>, result_norm.
using RefDump = std::map<std::string, std::vector<float>>;

// Evaluates n tokens at positions n_past..n_past+n-1 and returns the softcapped
// logits of the last one (vocab-sized) in `logits`.
void RefForward(const E4bModel& model, RefState* state, const int32_t* tokens, int n,
    std::vector<float>* logits, RefDump* dump = nullptr);

}  // namespace liminal::engine
