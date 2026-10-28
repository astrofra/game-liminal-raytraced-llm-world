#include "core/sampler.h"

#include <algorithm>
#include <cmath>

namespace liminal::engine {

namespace {

static bool Allowed(const uint32_t* allowed, int32_t id)
{
    return !allowed || (allowed[id >> 5] >> (id & 31)) & 1u;
}

}  // namespace

void SamplerInit(Sampler* sampler, const SamplerConfig& config)
{
    sampler->config = config;
    sampler->rng.seed(config.seed);
}

int32_t SamplerPick(Sampler* sampler, const float* logits, int vocab_size, const uint32_t* allowed)
{
    const SamplerConfig& config = sampler->config;
    if (config.temperature <= 0.0f) {
        int32_t best = -1;
        for (int32_t id = 0; id < vocab_size; ++id)
            if (Allowed(allowed, id) && (best < 0 || logits[id] > logits[best])) best = id;
        return best;
    }
    auto& ids = sampler->ids;
    ids.clear();
    for (int32_t id = 0; id < vocab_size; ++id) if (Allowed(allowed, id)) ids.push_back(id);
    if (ids.empty()) return -1;
    const auto by_logit = [&](int32_t a, int32_t b) { return logits[a] > logits[b] || (logits[a] == logits[b] && a < b); };
    if (config.top_k > 0 && static_cast<size_t>(config.top_k) < ids.size()) {
        std::partial_sort(ids.begin(), ids.begin() + config.top_k, ids.end(), by_logit);
        ids.resize(static_cast<size_t>(config.top_k));
    } else {
        std::sort(ids.begin(), ids.end(), by_logit);
    }
    // top-p over the softmax of the remaining (untempered) logits, keeping at least one.
    auto& weights = sampler->weights;
    weights.resize(ids.size());
    const float max_logit = logits[ids[0]];
    double sum = 0.0;
    for (size_t i = 0; i < ids.size(); ++i) { weights[i] = std::exp(logits[ids[i]] - max_logit); sum += weights[i]; }
    if (config.top_p < 1.0f) {
        double cumulative = 0.0;
        size_t keep = ids.size();
        for (size_t i = 0; i < ids.size(); ++i) {
            cumulative += weights[i] / sum;
            if (cumulative >= config.top_p) { keep = i + 1; break; }
        }
        ids.resize(keep);
    }
    // Temperature, then a draw from the renormalized distribution.
    weights.resize(ids.size());
    sum = 0.0;
    for (size_t i = 0; i < ids.size(); ++i) {
        weights[i] = std::exp((logits[ids[i]] - max_logit) / config.temperature);
        sum += weights[i];
    }
    std::uniform_real_distribution<double> uniform(0.0, sum);
    double target = uniform(sampler->rng);
    for (size_t i = 0; i < ids.size(); ++i) {
        target -= weights[i];
        if (target <= 0.0) return ids[i];
    }
    return ids.back();
}

}  // namespace liminal::engine
