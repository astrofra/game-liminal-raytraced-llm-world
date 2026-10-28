#include "ref/forward_ref.h"

#include "core/quant_ref.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace liminal::engine {

namespace {

static float Dot(const float* a, const float* b, int64_t n)
{
    float acc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int64_t i = 0;
    for (; i + 8 <= n; i += 8)
        for (int j = 0; j < 8; ++j) acc[j] += a[i + j] * b[i + j];
    float sum = ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
    for (; i < n; ++i) sum += a[i] * b[i];
    return sum;
}

// y[t][r] = W[r] . x[t] for n tokens; each weight row is dequantized once.
static void MatMul(const WeightRef& w, const float* x, int n, float* y)
{
    const int64_t cols = w.cols, rows = w.rows;
#pragma omp parallel
    {
        std::vector<float> row(static_cast<size_t>(cols));
#pragma omp for schedule(static)
        for (int64_t r = 0; r < rows; ++r) {
            DequantizeWeightRow(w, r, row.data());
            for (int t = 0; t < n; ++t) y[static_cast<int64_t>(t) * rows + r] = Dot(row.data(), x + static_cast<int64_t>(t) * cols, cols);
        }
    }
}

static const float* F32(const WeightRef& w) { return reinterpret_cast<const float*>(w.data); }

// In place: v = v / sqrt(mean(v^2) + eps) * weight (weight may be null).
static void RmsNorm(float* v, int64_t n, const float* weight)
{
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i) sum += static_cast<double>(v[i]) * v[i];
    const float scale = 1.0f / std::sqrt(static_cast<float>(sum / static_cast<double>(n)) + kE4bRmsEps);
    for (int64_t i = 0; i < n; ++i) v[i] = v[i] * scale * (weight ? weight[i] : 1.0f);
}

static float GeluTanh(float x)
{
    const float c = 0.7978845608028654f;  // sqrt(2/pi)
    return 0.5f * x * (1.0f + std::tanh(c * (x + 0.044715f * x * x * x)));
}

// NEOX rotation of one head (pairs i, i + d/2) at `pos`.
static void Rope(float* x, int dims, float base, const float* factors, int pos)
{
    const int half = dims / 2;
    for (int i = 0; i < half; ++i) {
        float theta = static_cast<float>(pos) * std::pow(base, -2.0f * static_cast<float>(i) / static_cast<float>(dims));
        if (factors) theta /= factors[i];
        const float c = std::cos(theta), s = std::sin(theta);
        const float x0 = x[i], x1 = x[i + half];
        x[i] = x0 * c - x1 * s;
        x[i + half] = x0 * s + x1 * c;
    }
}

static void Keep(RefDump* dump, const std::string& name, const float* last, int64_t n)
{
    if (dump) (*dump)[name].assign(last, last + n);
}

}  // namespace

void RefForward(const E4bModel& model, RefState* state, const int32_t* tokens, int n,
    std::vector<float>* logits, RefDump* dump)
{
    const int64_t E = kE4bEmbd;
    const int64_t P = kE4bPleDim * kE4bLayers;
    const int p0 = state->n_past;
    std::vector<float> x(static_cast<size_t>(n * E));
    std::vector<float> pt(static_cast<size_t>(n * P)), pp(static_cast<size_t>(n * P));
    for (int t = 0; t < n; ++t) {
        DequantizeWeightRow(model.token_embd, tokens[t], &x[t * E]);
        for (int64_t i = 0; i < E; ++i) x[t * E + i] *= std::sqrt(static_cast<float>(E));
        DequantizeWeightRow(model.per_layer_token_embd, tokens[t], &pt[t * P]);
        for (int64_t i = 0; i < P; ++i) pt[t * P + i] *= std::sqrt(static_cast<float>(kE4bPleDim));
    }
    Keep(dump, "inp_scaled", &x[(n - 1) * E], E);
    MatMul(model.per_layer_model_proj, x.data(), n, pp.data());
    for (int t = 0; t < n; ++t) {
        for (int l = 0; l < kE4bLayers; ++l) {
            float* slice = &pp[t * P + l * kE4bPleDim];
            for (int i = 0; i < kE4bPleDim; ++i) slice[i] /= std::sqrt(static_cast<float>(E));
            RmsNorm(slice, kE4bPleDim, F32(model.per_layer_proj_norm));
            const float* embd = &pt[t * P + l * kE4bPleDim];
            for (int i = 0; i < kE4bPleDim; ++i) slice[i] = (slice[i] + embd[i]) / std::sqrt(2.0f);
        }
    }

    std::vector<float> h(static_cast<size_t>(n * E)), proj(static_cast<size_t>(n * E));
    std::vector<float> q, k, v, o, gate(static_cast<size_t>(n * kE4bFfn)), up(static_cast<size_t>(n * kE4bFfn));
    std::vector<float> pg(static_cast<size_t>(n * kE4bPleDim));
    for (int l = 0; l < kE4bLayers; ++l) {
        const E4bLayer& L = model.layers[l];
        const int hd = L.head_dim;
        const float* factors = L.swa ? nullptr : model.rope_freq_factors.data();
        h = x;
        for (int t = 0; t < n; ++t) RmsNorm(&h[t * E], E, F32(L.attn_norm));
        q.resize(static_cast<size_t>(n * L.q_dim));
        MatMul(L.wq, h.data(), n, q.data());
        for (int t = 0; t < n; ++t)
            for (int head = 0; head < kE4bHeads; ++head) {
                float* qh = &q[t * L.q_dim + head * hd];
                RmsNorm(qh, hd, F32(L.q_norm));
                Rope(qh, hd, L.rope_base, factors, p0 + t);
            }
        if (L.owns_kv) {
            k.resize(static_cast<size_t>(n * L.kv_dim));
            v.resize(static_cast<size_t>(n * L.kv_dim));
            MatMul(L.wk, h.data(), n, k.data());
            MatMul(L.wv, h.data(), n, v.data());
            for (int t = 0; t < n; ++t)
                for (int head = 0; head < kE4bKvHeads; ++head) {
                    float* kh = &k[t * L.kv_dim + head * hd];
                    RmsNorm(kh, hd, F32(L.k_norm));
                    Rope(kh, hd, L.rope_base, factors, p0 + t);
                    RmsNorm(&v[t * L.kv_dim + head * hd], hd, nullptr);
                }
            state->k[l].insert(state->k[l].end(), k.begin(), k.end());
            state->v[l].insert(state->v[l].end(), v.begin(), v.end());
        }
        const std::vector<float>& K = state->k[L.kv_source];
        const std::vector<float>& V = state->v[L.kv_source];
        o.assign(static_cast<size_t>(n * L.q_dim), 0.0f);
#pragma omp parallel for collapse(2) schedule(dynamic)
        for (int t = 0; t < n; ++t)
            for (int head = 0; head < kE4bHeads; ++head) {
                const int pos = p0 + t;
                const int kv_head = head / (kE4bHeads / kE4bKvHeads);
                const int first = L.swa ? std::max(0, pos - kE4bSlidingWindow + 1) : 0;
                const float* qh = &q[t * L.q_dim + head * hd];
                std::vector<float> scores(static_cast<size_t>(pos - first + 1));
                float max_score = -INFINITY;
                for (int j = first; j <= pos; ++j) {
                    const float s = Dot(qh, &K[static_cast<size_t>(j) * L.kv_dim + kv_head * hd], hd);
                    scores[static_cast<size_t>(j - first)] = s;
                    max_score = std::max(max_score, s);
                }
                double sum = 0.0;
                for (auto& s : scores) { s = std::exp(s - max_score); sum += s; }
                float* oh = &o[t * L.q_dim + head * hd];
                for (int j = first; j <= pos; ++j) {
                    const float w = static_cast<float>(scores[static_cast<size_t>(j - first)] / sum);
                    const float* vh = &V[static_cast<size_t>(j) * L.kv_dim + kv_head * hd];
                    for (int i = 0; i < hd; ++i) oh[i] += w * vh[i];
                }
            }
        MatMul(L.wo, o.data(), n, proj.data());
        for (int t = 0; t < n; ++t) {
            RmsNorm(&proj[t * E], E, F32(L.post_attention_norm));
            for (int64_t i = 0; i < E; ++i) x[t * E + i] += proj[t * E + i];
        }
        Keep(dump, "attn_post_norm-" + std::to_string(l), &proj[(n - 1) * E], E);

        h = x;
        for (int t = 0; t < n; ++t) RmsNorm(&h[t * E], E, F32(L.ffn_norm));
        MatMul(L.w_gate, h.data(), n, gate.data());
        MatMul(L.w_up, h.data(), n, up.data());
        for (size_t i = 0; i < gate.size(); ++i) gate[i] = GeluTanh(gate[i]) * up[i];
        MatMul(L.w_down, gate.data(), n, proj.data());
        for (int t = 0; t < n; ++t) {
            RmsNorm(&proj[t * E], E, F32(L.post_ffw_norm));
            for (int64_t i = 0; i < E; ++i) x[t * E + i] += proj[t * E + i];
        }
        Keep(dump, "ffn_post_norm-" + std::to_string(l), &proj[(n - 1) * E], E);

        MatMul(L.inp_gate, x.data(), n, pg.data());
        for (int t = 0; t < n; ++t)
            for (int i = 0; i < kE4bPleDim; ++i)
                pg[t * kE4bPleDim + i] = GeluTanh(pg[t * kE4bPleDim + i]) * pp[t * P + l * kE4bPleDim + i];
        MatMul(L.proj, pg.data(), n, proj.data());
        for (int t = 0; t < n; ++t) {
            RmsNorm(&proj[t * E], E, F32(L.post_norm));
            for (int64_t i = 0; i < E; ++i) x[t * E + i] = (x[t * E + i] + proj[t * E + i]) * L.output_scale;
        }
        Keep(dump, "per_layer_embd_out-" + std::to_string(l), &proj[(n - 1) * E], E);
        Keep(dump, "l_out-" + std::to_string(l), &x[(n - 1) * E], E);
    }
    state->n_past += n;

    std::vector<float> last(x.end() - E, x.end());
    RmsNorm(last.data(), E, F32(model.output_norm));
    Keep(dump, "result_norm", last.data(), E);
    logits->resize(kE4bVocab);
    MatVecRef(model.token_embd, last.data(), logits->data());
    for (auto& value : *logits) value = kE4bLogitSoftcap * std::tanh(value / kE4bLogitSoftcap);
}

}  // namespace liminal::engine
