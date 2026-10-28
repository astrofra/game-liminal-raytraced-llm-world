#include "core/e4b_spec.h"

#include <cmath>
#include <cstring>

namespace liminal::engine {

namespace {

struct Binder {
    const GgufFile& file;
    std::string* error;
    bool ok = true;

    bool Fail(const std::string& message) {
        if (ok) *error = message;
        ok = false;
        return false;
    }

    // Binds a tensor of `cols x rows` whose type is one of `types` (0-terminated).
    bool Bind(const std::string& name, int64_t cols, int64_t rows, std::initializer_list<uint32_t> types, WeightRef* out) {
        const GgufTensor* tensor = GgufFindTensor(file, name);
        if (!tensor) return Fail("Missing tensor " + name);
        bool type_ok = false;
        for (uint32_t type : types) type_ok |= tensor->type == type;
        if (!type_ok) return Fail("Unexpected type " + std::string(GgmlTypeName(tensor->type)) + " for " + name);
        const int64_t tensor_rows = tensor->ne[1] * tensor->ne[2] * tensor->ne[3];
        if (tensor->ne[0] != cols || tensor_rows != rows)
            return Fail("Unexpected shape for " + name + ": " + std::to_string(tensor->ne[0]) + "x" + std::to_string(tensor_rows));
        out->type = tensor->type;
        out->cols = cols;
        out->rows = rows;
        out->row_bytes = tensor->row_bytes;
        out->data = tensor->data;
        return true;
    }

    bool ExpectU32(const char* key, uint32_t expected) {
        uint32_t value = 0;
        if (!GgufGetU32(file, key, &value) || value != expected)
            return Fail(std::string("Unexpected ") + key + " (expected " + std::to_string(expected) + ")");
        return true;
    }

    bool ExpectF32(const char* key, float expected) {
        float value = 0.0f;
        if (!GgufGetF32(file, key, &value) || std::fabs(value - expected) > std::fabs(expected) * 1e-6f)
            return Fail(std::string("Unexpected ") + key);
        return true;
    }
};

static float ReadF32(const WeightRef& ref, int64_t index)
{
    float value = 0.0f;
    memcpy(&value, ref.data + index * 4, 4);
    return value;
}

}  // namespace

bool E4bBind(const GgufFile& file, E4bModel* model, std::string* error)
{
    Binder b{file, error};
    std::string_view text;
    if (!GgufGetString(file, "general.architecture", &text) || text != "gemma4") return b.Fail("Model architecture is not gemma4");
    if (!GgufGetString(file, "tokenizer.ggml.model", &text) || text != "gemma4") return b.Fail("Tokenizer is not gemma4");
    b.ExpectU32("gemma4.block_count", kE4bLayers);
    b.ExpectU32("gemma4.embedding_length", kE4bEmbd);
    b.ExpectU32("gemma4.feed_forward_length", kE4bFfn);
    b.ExpectU32("gemma4.attention.head_count", kE4bHeads);
    b.ExpectU32("gemma4.attention.head_count_kv", kE4bKvHeads);
    b.ExpectU32("gemma4.attention.key_length", kE4bHeadDimFull);
    b.ExpectU32("gemma4.attention.value_length", kE4bHeadDimFull);
    b.ExpectU32("gemma4.attention.key_length_swa", kE4bHeadDimSwa);
    b.ExpectU32("gemma4.attention.value_length_swa", kE4bHeadDimSwa);
    b.ExpectU32("gemma4.rope.dimension_count", kE4bHeadDimFull);
    b.ExpectU32("gemma4.rope.dimension_count_swa", kE4bHeadDimSwa);
    b.ExpectU32("gemma4.attention.sliding_window", kE4bSlidingWindow);
    b.ExpectU32("gemma4.attention.shared_kv_layers", kE4bLayers - kE4bKvLayers);
    b.ExpectU32("gemma4.embedding_length_per_layer_input", kE4bPleDim);
    b.ExpectF32("gemma4.rope.freq_base", kE4bRopeBaseFull);
    b.ExpectF32("gemma4.rope.freq_base_swa", kE4bRopeBaseSwa);
    b.ExpectF32("gemma4.attention.layer_norm_rms_epsilon", kE4bRmsEps);
    b.ExpectF32("gemma4.final_logit_softcapping", kE4bLogitSoftcap);
    if (!b.ok) return false;

    const GgufKv* pattern = GgufFind(file, "gemma4.attention.sliding_window_pattern");
    if (!pattern || pattern->type != kGgufArray || pattern->array_type != kGgufBool || pattern->array_count != kE4bLayers)
        return b.Fail("Missing sliding_window_pattern");

    const std::initializer_list<uint32_t> quantized = {kGgmlQ4K, kGgmlQ6K};
    const std::initializer_list<uint32_t> f32 = {kGgmlF32};
    b.Bind("token_embd.weight", kE4bEmbd, kE4bVocab, {kGgmlQ6K}, &model->token_embd);
    b.Bind("per_layer_token_embd.weight", kE4bPleDim * kE4bLayers, kE4bVocab, {kGgmlQ6K}, &model->per_layer_token_embd);
    b.Bind("per_layer_model_proj.weight", kE4bEmbd, kE4bPleDim * kE4bLayers, {kGgmlBF16}, &model->per_layer_model_proj);
    b.Bind("per_layer_proj_norm.weight", kE4bPleDim, 1, f32, &model->per_layer_proj_norm);
    b.Bind("output_norm.weight", kE4bEmbd, 1, f32, &model->output_norm);
    WeightRef rope_freqs;
    b.Bind("rope_freqs.weight", kE4bHeadDimFull / 2, 1, f32, &rope_freqs);
    if (GgufFindTensor(file, "output.weight")) return b.Fail("Unexpected untied output.weight");
    if (!b.ok) return false;
    model->rope_freq_factors.resize(kE4bHeadDimFull / 2);
    for (int i = 0; i < kE4bHeadDimFull / 2; ++i) model->rope_freq_factors[i] = ReadF32(rope_freqs, i);

    for (int l = 0; l < kE4bLayers; ++l) {
        E4bLayer& layer = model->layers[l];
        layer.swa = pattern->array_data[l] != 0;
        layer.head_dim = layer.swa ? kE4bHeadDimSwa : kE4bHeadDimFull;
        layer.q_dim = kE4bHeads * layer.head_dim;
        layer.kv_dim = kE4bKvHeads * layer.head_dim;
        layer.rope_base = layer.swa ? kE4bRopeBaseSwa : kE4bRopeBaseFull;
        layer.owns_kv = l < kE4bKvLayers;
        // llama-model.cpp: shared layers read n_layer_kv_from_start - (swa ? 2 : 1).
        layer.kv_source = layer.owns_kv ? l : kE4bKvLayers - (layer.swa ? 2 : 1);
        const std::string p = "blk." + std::to_string(l) + ".";
        b.Bind(p + "attn_norm.weight", kE4bEmbd, 1, f32, &layer.attn_norm);
        b.Bind(p + "attn_q.weight", kE4bEmbd, layer.q_dim, quantized, &layer.wq);
        b.Bind(p + "attn_k.weight", kE4bEmbd, layer.kv_dim, quantized, &layer.wk);
        b.Bind(p + "attn_v.weight", kE4bEmbd, layer.kv_dim, quantized, &layer.wv);
        b.Bind(p + "attn_q_norm.weight", layer.head_dim, 1, f32, &layer.q_norm);
        b.Bind(p + "attn_k_norm.weight", layer.head_dim, 1, f32, &layer.k_norm);
        b.Bind(p + "attn_output.weight", layer.q_dim, kE4bEmbd, quantized, &layer.wo);
        b.Bind(p + "post_attention_norm.weight", kE4bEmbd, 1, f32, &layer.post_attention_norm);
        b.Bind(p + "ffn_norm.weight", kE4bEmbd, 1, f32, &layer.ffn_norm);
        b.Bind(p + "ffn_gate.weight", kE4bEmbd, kE4bFfn, quantized, &layer.w_gate);
        b.Bind(p + "ffn_up.weight", kE4bEmbd, kE4bFfn, quantized, &layer.w_up);
        b.Bind(p + "ffn_down.weight", kE4bFfn, kE4bEmbd, quantized, &layer.w_down);
        b.Bind(p + "post_ffw_norm.weight", kE4bEmbd, 1, f32, &layer.post_ffw_norm);
        b.Bind(p + "inp_gate.weight", kE4bEmbd, kE4bPleDim, quantized, &layer.inp_gate);
        b.Bind(p + "proj.weight", kE4bPleDim, kE4bEmbd, quantized, &layer.proj);
        b.Bind(p + "post_norm.weight", kE4bEmbd, 1, f32, &layer.post_norm);
        WeightRef scale;
        b.Bind(p + "layer_output_scale.weight", 1, 1, f32, &scale);
        if (!b.ok) return false;
        layer.output_scale = ReadF32(scale, 0);
    }
    // The KV sources must be the last owning layer of the same attention type.
    for (int l = kE4bKvLayers; l < kE4bLayers; ++l) {
        const E4bLayer& layer = model->layers[l];
        if (model->layers[layer.kv_source].swa != layer.swa) return b.Fail("Inconsistent KV sharing pattern");
    }
    return true;
}

}  // namespace liminal::engine
