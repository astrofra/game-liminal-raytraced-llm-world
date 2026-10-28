#pragma once
// Token selection with llama.cpp's game chain: top-k -> top-p -> temperature ->
// draw, or argmax when temperature is 0. An optional allow-mask (JSON constraint)
// is applied first. Same seed and logits give the same token.
#include <cstdint>
#include <random>
#include <vector>

namespace liminal::engine {

struct SamplerConfig {
    float temperature = 0.0f;
    int top_k = 40;
    float top_p = 0.95f;
    uint32_t seed = 42;
};

struct Sampler {
    SamplerConfig config;
    std::mt19937 rng;
    std::vector<int32_t> ids;     // scratch
    std::vector<float> weights;   // scratch
};

void SamplerInit(Sampler* sampler, const SamplerConfig& config);
// allowed: nullptr, or one bit per token (bit set = allowed). Returns -1 when nothing is allowed.
int32_t SamplerPick(Sampler* sampler, const float* logits, int vocab_size, const uint32_t* allowed);

}  // namespace liminal::engine
