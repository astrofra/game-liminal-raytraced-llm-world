#include "mlx/mlx_backend.h"

#include "core/gguf.h"

#include <mlx/mlx.h>

#include <cmath>
#include <cstring>
#include <memory>

namespace liminal::engine {

namespace mx = mlx::core;

namespace {

// Metal helpers shared by the K-quant kernels. Block layouts are ggml's:
// Q4_K = f16 d, f16 dmin, scales[12], qs[128] (144 bytes / 256 values)
// Q6_K = ql[128], qh[64], int8 scales[16], f16 d (210 bytes / 256 values)
// The tiled GEMM and its subblock loaders come from libinfer's kquant.cpp.
const char* kHeader = R"METAL(
template <typename T, int N>
METAL_FUNC simdgroup_matrix<T, N, N> filled_simdgroup_matrix(T value) {
	simdgroup_matrix<T, N, N> result;
	result.thread_elements() = value;
	return result;
}

METAL_FUNC float half_from_bytes(device const uint8_t* bytes) {
	return float(*reinterpret_cast<device const half*>(bytes));
}

METAL_FUNC float decode_q6k(device const uint8_t* row, uint column) {
	device const uint8_t* block = row + (column / 256) * 210;
	const uint index = column % 256;
	const uint cluster_index = index % 128;
	const uint cluster = index / 128;
	device const uint8_t* low = block + cluster * 64;
	device const uint8_t* high = block + 128 + cluster * 32;
	const uint low_bits = (low[cluster_index % 64] >> ((cluster_index / 64) * 4)) & 15;
	const uint high_bits = ((high[cluster_index % 32] >> ((cluster_index / 32) * 2)) & 3) << 4;
	const int quant = int(low_bits | high_bits) - 32;
	const int scale = int(reinterpret_cast<device const int8_t*>(block + 192)[index / 16]);
	return half_from_bytes(block + 208) * float(scale * quant);
}

template <typename U>
METAL_FUNC void load_q4k_subblock(device const uint8_t* row, uint k_base, uint output_column, threadgroup U* tile) {
	device const uint8_t* block = row + (k_base / 256) * 144;
	const uint subblock = (k_base % 256) / 32;
	device const uint8_t* scales = block + 4;
	uint scale;
	uint minimum;
	if (subblock < 4) {
		scale = scales[subblock] & 63;
		minimum = scales[subblock + 4] & 63;
	} else {
		scale = (scales[subblock + 4] & 15) | ((scales[subblock - 4] >> 6) << 4);
		minimum = (scales[subblock + 4] >> 4) | ((scales[subblock] >> 6) << 4);
	}
	const float quant_scale = half_from_bytes(block) * float(scale);
	const float quant_minimum = half_from_bytes(block + 2) * float(minimum);
	device const uint8_t* packed = block + 16 + (subblock / 2) * 32;
	for (uint offset = 0; offset < 32; ++offset) {
		const uint quant = (subblock & 1) == 0 ? packed[offset] & 15 : packed[offset] >> 4;
		tile[offset * 64 + output_column] = U(quant_scale * float(quant) - quant_minimum);
	}
}

template <typename U>
METAL_FUNC void load_q6k_subblock(device const uint8_t* row, uint k_base, uint output_column, threadgroup U* tile) {
	device const uint8_t* block = row + (k_base / 256) * 210;
	const uint subblock = (k_base % 256) / 32;
	const uint cluster = subblock / 4;
	const uint cluster_subblock = subblock % 4;
	device const uint8_t* low = block + cluster * 64;
	device const uint8_t* high = block + 128 + cluster * 32;
	device const int8_t* scales = reinterpret_cast<device const int8_t*>(block + 192);
	const float delta = half_from_bytes(block + 208);
	for (uint offset = 0; offset < 32; ++offset) {
		const uint cluster_index = cluster_subblock * 32 + offset;
		const uint low_bits = (low[cluster_index % 64] >> ((cluster_index / 64) * 4)) & 15;
		const uint high_bits = ((high[offset] >> (cluster_subblock * 2)) & 3) << 4;
		const int quant = int(low_bits | high_bits) - 32;
		const int scale = int(scales[(subblock * 32 + offset) / 16]);
		tile[offset * 64 + output_column] = U(delta * float(scale * quant));
	}
}
)METAL";

// out[N] = W . x for one f32 activation row. Each simdgroup owns 4 weight rows;
// lane l covers values [8l, 8l + 8) of every 256-value block.
const char* kGemvQ4K = R"METAL(
	const uint NR = 4;
	const uint lane = thread_index_in_simdgroup;
	const uint sg = simdgroup_index_in_threadgroup;
	const uint sg_count = threads_per_threadgroup.x / 32;
	const uint n_rows = uint(w_shape[0]);
	const uint row_bytes = uint(w_shape[1]);
	const uint nblocks = row_bytes / 144;
	const uint first = (threadgroup_position_in_grid.x * sg_count + sg) * NR;
	const uint s = lane / 4;
	const uint j0 = (lane % 4) * 8;
	const uint shift = (s & 1) * 4;
	float acc[NR] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (uint b = 0; b < nblocks; ++b) {
		const uint xo = b * 256 + s * 32 + j0;
		float xv[8];
		float xsum = 0.0f;
		for (uint i = 0; i < 8; ++i) { xv[i] = float(x[xo + i]); xsum += xv[i]; }
		for (uint r = 0; r < NR; ++r) {
			const uint row = min(first + r, n_rows - 1);
			device const uint8_t* block = w + row * row_bytes + b * 144;
			device const uint8_t* sc = block + 4;
			uint scale, mn;
			if (s < 4) { scale = sc[s] & 63; mn = sc[s + 4] & 63; }
			else { scale = (sc[s + 4] & 15) | ((sc[s - 4] >> 6) << 4); mn = (sc[s + 4] >> 4) | ((sc[s] >> 6) << 4); }
			device const uint8_t* q = block + 16 + (s / 2) * 32 + j0;
			float dot = 0.0f;
			for (uint i = 0; i < 8; ++i) dot += float((q[i] >> shift) & 15) * xv[i];
			acc[r] += half_from_bytes(block) * float(scale) * dot - half_from_bytes(block + 2) * float(mn) * xsum;
		}
	}
	for (uint r = 0; r < NR; ++r) {
		const float total = simd_sum(acc[r]);
		if (lane == 0 && first + r < n_rows) out[first + r] = total;
	}
)METAL";

const char* kGemvQ6K = R"METAL(
	const uint NR = 4;
	const uint lane = thread_index_in_simdgroup;
	const uint sg = simdgroup_index_in_threadgroup;
	const uint sg_count = threads_per_threadgroup.x / 32;
	const uint n_rows = uint(w_shape[0]);
	const uint row_bytes = uint(w_shape[1]);
	const uint nblocks = row_bytes / 210;
	const uint first = (threadgroup_position_in_grid.x * sg_count + sg) * NR;
	const uint i0 = lane * 8;
	const uint cluster = i0 / 128;
	const uint ci = i0 % 128;
	const uint low_offset = cluster * 64 + ci % 64;
	const uint low_shift = (ci / 64) * 4;
	const uint high_offset = 128 + cluster * 32 + ci % 32;
	const uint high_shift = (ci / 32) * 2;
	const uint scale_offset = 192 + i0 / 16;
	float acc[NR] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (uint b = 0; b < nblocks; ++b) {
		const uint xo = b * 256 + i0;
		float xv[8];
		for (uint i = 0; i < 8; ++i) xv[i] = float(x[xo + i]);
		for (uint r = 0; r < NR; ++r) {
			const uint row = min(first + r, n_rows - 1);
			device const uint8_t* block = w + row * row_bytes + b * 210;
			device const uint8_t* low = block + low_offset;
			device const uint8_t* high = block + high_offset;
			float dot = 0.0f;
			for (uint i = 0; i < 8; ++i) {
				const int quant = int(((low[i] >> low_shift) & 15) | (((high[i] >> high_shift) & 3) << 4)) - 32;
				dot += float(quant) * xv[i];
			}
			acc[r] += half_from_bytes(block + 208) * float(reinterpret_cast<device const int8_t*>(block)[scale_offset]) * dot;
		}
	}
	for (uint r = 0; r < NR; ++r) {
		const float total = simd_sum(acc[r]);
		if (lane == 0 && first + r < n_rows) out[first + r] = total;
	}
)METAL";

// Tiled 64x64 GEMM over K-quant rows (libinfer kquant.cpp); T is the activation type.
const char* kGemmBody = R"METAL(
	const uint m_count = uint(x_shape[0]);
	const uint k_count = uint(x_shape[1]);
	const uint n_count = uint(w_shape[0]);
	const uint row_bytes = uint(w_shape[1]);
	const uint m_base = threadgroup_position_in_grid.y * 64;
	const uint n_base = threadgroup_position_in_grid.x * 64;
	const uint local_thread = thread_index_in_threadgroup;
	const uint simdgroup = simdgroup_index_in_threadgroup;

	threadgroup T a_tile[64 * 32];
	threadgroup T b_tile[32 * 64];
	threadgroup float c_tile[64 * 64];

	simdgroup_float8x8 accumulators[8];
	for (uint index = 0; index < 8; ++index) accumulators[index] = filled_simdgroup_matrix<float, 8>(0.0f);

	for (uint k_base = 0; k_base < k_count; k_base += 32) {
		for (uint index = local_thread; index < 64 * 32; index += 256) {
			const uint row = index / 32;
			const uint column = index % 32;
			const uint global_m = m_base + row;
			const uint global_k = k_base + column;
			a_tile[index] = global_m < m_count && global_k < k_count ? T(x[global_m * k_count + global_k]) : T(0);
		}
		if (local_thread < 64) {
			const uint global_n = n_base + local_thread;
			if (global_n < n_count) {
				LOAD_SUBBLOCK(w + global_n * row_bytes, k_base, local_thread, b_tile);
			} else {
				for (uint offset = 0; offset < 32; ++offset) b_tile[offset * 64 + local_thread] = T(0);
			}
		}
		threadgroup_barrier(mem_flags::mem_threadgroup);

		const uint output_row = (simdgroup / 2) * 16;
		const uint output_column = (simdgroup % 2) * 32;
		for (uint k_tile = 0; k_tile < 4; ++k_tile) {
			simdgroup_matrix<T, 8, 8> a_top, a_bottom, b_0, b_1, b_2, b_3;
			simdgroup_load(a_top, a_tile + output_row * 32 + k_tile * 8, 32);
			simdgroup_load(a_bottom, a_tile + (output_row + 8) * 32 + k_tile * 8, 32);
			simdgroup_load(b_0, b_tile + k_tile * 8 * 64 + output_column, 64);
			simdgroup_load(b_1, b_tile + k_tile * 8 * 64 + output_column + 8, 64);
			simdgroup_load(b_2, b_tile + k_tile * 8 * 64 + output_column + 16, 64);
			simdgroup_load(b_3, b_tile + k_tile * 8 * 64 + output_column + 24, 64);
			simdgroup_multiply_accumulate(accumulators[0], a_top, b_0, accumulators[0]);
			simdgroup_multiply_accumulate(accumulators[1], a_top, b_1, accumulators[1]);
			simdgroup_multiply_accumulate(accumulators[2], a_top, b_2, accumulators[2]);
			simdgroup_multiply_accumulate(accumulators[3], a_top, b_3, accumulators[3]);
			simdgroup_multiply_accumulate(accumulators[4], a_bottom, b_0, accumulators[4]);
			simdgroup_multiply_accumulate(accumulators[5], a_bottom, b_1, accumulators[5]);
			simdgroup_multiply_accumulate(accumulators[6], a_bottom, b_2, accumulators[6]);
			simdgroup_multiply_accumulate(accumulators[7], a_bottom, b_3, accumulators[7]);
		}
		threadgroup_barrier(mem_flags::mem_threadgroup);
	}

	const uint output_row = (simdgroup / 2) * 16;
	const uint output_column = (simdgroup % 2) * 32;
	simdgroup_store(accumulators[0], c_tile + output_row * 64 + output_column, 64);
	simdgroup_store(accumulators[1], c_tile + output_row * 64 + output_column + 8, 64);
	simdgroup_store(accumulators[2], c_tile + output_row * 64 + output_column + 16, 64);
	simdgroup_store(accumulators[3], c_tile + output_row * 64 + output_column + 24, 64);
	simdgroup_store(accumulators[4], c_tile + (output_row + 8) * 64 + output_column, 64);
	simdgroup_store(accumulators[5], c_tile + (output_row + 8) * 64 + output_column + 8, 64);
	simdgroup_store(accumulators[6], c_tile + (output_row + 8) * 64 + output_column + 16, 64);
	simdgroup_store(accumulators[7], c_tile + (output_row + 8) * 64 + output_column + 24, 64);
	threadgroup_barrier(mem_flags::mem_threadgroup);

	for (uint index = local_thread; index < 64 * 64; index += 256) {
		const uint global_m = m_base + index / 64;
		const uint global_n = n_base + index % 64;
		if (global_m < m_count && global_n < n_count) out[global_m * n_count + global_n] = float(c_tile[index]);
	}
)METAL";

// One thread per output value: rows of a Q6_K table gathered by token id.
const char* kEmbedQ6K = R"METAL(
	const uint output_index = thread_position_in_grid.x;
	const uint row_bytes = uint(w_shape[1]);
	const uint dimensions = row_bytes / 210 * 256;
	if (output_index >= uint(ids_shape[0]) * dimensions) return;
	const uint row = uint(ids[output_index / dimensions]);
	out[output_index] = decode_q6k(w + row * row_bytes, output_index % dimensions);
)METAL";

struct QWeight {
    mx::array bytes = mx::array(0);
    bool q6 = false;
    int rows = 0;
    int cols = 0;
};

struct Kernels {
    mx::fast::CustomKernelFunction gemv_q4k = mx::fast::metal_kernel("liminal_gemv_q4k", {"x", "w"}, {"out"}, kGemvQ4K, kHeader);
    mx::fast::CustomKernelFunction gemv_q6k = mx::fast::metal_kernel("liminal_gemv_q6k", {"x", "w"}, {"out"}, kGemvQ6K, kHeader);
    mx::fast::CustomKernelFunction gemm_q4k = mx::fast::metal_kernel("liminal_gemm_q4k", {"x", "w"}, {"out"},
        std::string("#define LOAD_SUBBLOCK load_q4k_subblock\n") + kGemmBody, kHeader);
    mx::fast::CustomKernelFunction gemm_q6k = mx::fast::metal_kernel("liminal_gemm_q6k", {"x", "w"}, {"out"},
        std::string("#define LOAD_SUBBLOCK load_q6k_subblock\n") + kGemmBody, kHeader);
    mx::fast::CustomKernelFunction embed_q6k = mx::fast::metal_kernel("liminal_embed_q6k", {"w", "ids"}, {"out"}, kEmbedQ6K, kHeader);
};

struct Layer {
    bool swa = false;
    int head_dim = 0;
    bool owns_kv = false;
    int kv_source = 0;
    float output_scale = 1.0f;
    mx::array rope_freqs = mx::array(0);
    mx::array attn_norm = mx::array(0), q_norm = mx::array(0), k_norm = mx::array(0), post_attention_norm = mx::array(0);
    mx::array ffn_norm = mx::array(0), post_ffw_norm = mx::array(0), post_norm = mx::array(0);
    QWeight wq, wk, wv, wo, gate, up, down, inp_gate, proj;
};

static mx::array F32Vector(const WeightRef& w)
{
    return mx::array(reinterpret_cast<const float*>(w.data), {static_cast<int>(w.cols * w.rows)}, mx::float32);
}

static QWeight Quant(const WeightRef& w)
{
    QWeight q;
    q.q6 = w.type == kGgmlQ6K;
    q.rows = static_cast<int>(w.rows);
    q.cols = static_cast<int>(w.cols);
    q.bytes = mx::array(w.data, {q.rows, static_cast<int>(w.row_bytes)}, mx::uint8);
    return q;
}

}  // namespace

struct MlxModel {
    Kernels kernels;
    mx::array token_embd = mx::array(0);
    mx::array per_layer_token_embd = mx::array(0);
    mx::array per_layer_model_proj = mx::array(0);  // f32 [10752, 2560]
    mx::array per_layer_proj_norm = mx::array(0);
    mx::array output_norm = mx::array(0);
    Layer layers[kE4bLayers];
    std::string device;
};

struct MlxState {
    int capacity = 0;
    int n_past = 0;
    std::vector<mx::array> k, v;  // per owning layer: [1, 2, capacity, head_dim] f16
};

namespace {

static mx::array Gemv(MlxModel* m, const QWeight& w, const mx::array& x)
{
    const int sg_count = 2;
    const int rows_per_group = 4 * sg_count;
    const int groups = (w.rows + rows_per_group - 1) / rows_per_group;
    auto out = (w.q6 ? m->kernels.gemv_q6k : m->kernels.gemv_q4k)(
        {mx::reshape(mx::astype(x, mx::float32), {w.cols}), w.bytes}, {{w.rows}}, {mx::float32},
        {groups * 32 * sg_count, 1, 1}, {32 * sg_count, 1, 1}, {}, std::nullopt, false, mx::Device::gpu);
    return mx::reshape(out[0], {1, w.rows});
}

static mx::array Gemm(MlxModel* m, const QWeight& w, const mx::array& x)
{
    const int rows = x.shape(0);
    auto out = (w.q6 ? m->kernels.gemm_q6k : m->kernels.gemm_q4k)(
        {mx::astype(x, mx::float16), w.bytes}, {{rows, w.rows}}, {mx::float32},
        {((w.rows + 63) / 64) * 256, (rows + 63) / 64, 1}, {256, 1, 1}, {{"T", mx::float16}}, std::nullopt, false, mx::Device::gpu);
    return out[0];
}

// x: [n, cols] -> [n, rows]
static mx::array Linear(MlxModel* m, const QWeight& w, const mx::array& x)
{
    return x.shape(0) == 1 ? Gemv(m, w, x) : Gemm(m, w, x);
}

static mx::array EmbedQ6K(MlxModel* m, const mx::array& table, const mx::array& ids, int dims)
{
    const int n = ids.shape(0);
    auto out = m->kernels.embed_q6k({table, ids}, {{n * dims}}, {mx::float32}, {n * dims, 1, 1}, {256, 1, 1}, {},
        std::nullopt, false, mx::Device::gpu);
    return mx::reshape(out[0], {n, dims});
}

static mx::array GeluTanh(const mx::array& x)
{
    const float c = 0.7978845608028654f;
    return 0.5f * x * (1.0f + mx::tanh(c * (x + 0.044715f * x * x * x)));
}

static mx::array Rms(const mx::array& x, const mx::array* weight)
{
    return mx::fast::rms_norm(x, weight ? std::optional<mx::array>(*weight) : std::nullopt, kE4bRmsEps);
}

}  // namespace

MlxModel* MlxLoad(const E4bModel& model, std::string* error)
{
    try {
        std::unique_ptr<MlxModel> m(new MlxModel());
        m->token_embd = Quant(model.token_embd).bytes;
        m->per_layer_token_embd = Quant(model.per_layer_token_embd).bytes;
        const WeightRef& proj = model.per_layer_model_proj;
        m->per_layer_model_proj = mx::astype(mx::view(mx::array(reinterpret_cast<const uint16_t*>(proj.data),
            {static_cast<int>(proj.rows), static_cast<int>(proj.cols)}, mx::uint16), mx::bfloat16), mx::float32);
        m->per_layer_proj_norm = F32Vector(model.per_layer_proj_norm);
        m->output_norm = F32Vector(model.output_norm);
        std::vector<mx::array> uploads = {m->token_embd, m->per_layer_token_embd, m->per_layer_model_proj};
        for (int l = 0; l < kE4bLayers; ++l) {
            const E4bLayer& src = model.layers[l];
            Layer& dst = m->layers[l];
            dst.swa = src.swa;
            dst.head_dim = src.head_dim;
            dst.owns_kv = src.owns_kv;
            dst.kv_source = src.kv_source;
            dst.output_scale = src.output_scale;
            if (!src.swa) {
                // mx::fast::rope uses theta_i = pos / freqs[i]: freqs = base^(2i/d) * factor_i.
                std::vector<float> freqs(static_cast<size_t>(src.head_dim / 2));
                for (size_t i = 0; i < freqs.size(); ++i)
                    freqs[i] = std::pow(src.rope_base, 2.0f * static_cast<float>(i) / static_cast<float>(src.head_dim)) * model.rope_freq_factors[i];
                dst.rope_freqs = mx::array(freqs.data(), {static_cast<int>(freqs.size())}, mx::float32);
            }
            dst.attn_norm = F32Vector(src.attn_norm);
            dst.q_norm = F32Vector(src.q_norm);
            dst.k_norm = F32Vector(src.k_norm);
            dst.post_attention_norm = F32Vector(src.post_attention_norm);
            dst.ffn_norm = F32Vector(src.ffn_norm);
            dst.post_ffw_norm = F32Vector(src.post_ffw_norm);
            dst.post_norm = F32Vector(src.post_norm);
            dst.wq = Quant(src.wq);
            if (src.owns_kv) { dst.wk = Quant(src.wk); dst.wv = Quant(src.wv); }
            dst.wo = Quant(src.wo);
            dst.gate = Quant(src.w_gate);
            dst.up = Quant(src.w_up);
            dst.down = Quant(src.w_down);
            dst.inp_gate = Quant(src.inp_gate);
            dst.proj = Quant(src.proj);
            for (const QWeight* q : {&dst.wq, &dst.wo, &dst.gate, &dst.up, &dst.down, &dst.inp_gate, &dst.proj}) uploads.push_back(q->bytes);
            if (src.owns_kv) { uploads.push_back(dst.wk.bytes); uploads.push_back(dst.wv.bytes); }
        }
        mx::eval(uploads);
        m->device = "mlx-metal";
        return m.release();
    } catch (const std::exception& e) {
        *error = std::string("MLX load failed: ") + e.what();
        return nullptr;
    }
}

void MlxFree(MlxModel* model) { delete model; }

const char* MlxDeviceName(const MlxModel* model) { return model->device.c_str(); }

MlxState* MlxStateCreate(MlxModel* model, int capacity)
{
    auto* state = new MlxState();
    state->capacity = capacity;
    for (int l = 0; l < kE4bKvLayers; ++l) {
        const int hd = model->layers[l].head_dim;
        state->k.push_back(mx::zeros({1, kE4bKvHeads, capacity, hd}, mx::float16));
        state->v.push_back(mx::zeros({1, kE4bKvHeads, capacity, hd}, mx::float16));
    }
    mx::eval(state->k);
    mx::eval(state->v);
    return state;
}

void MlxStateFree(MlxState* state) { delete state; }

int MlxStatePast(const MlxState* state) { return state->n_past; }

bool MlxForward(MlxModel* m, MlxState* state, const int32_t* tokens, int n, std::vector<float>* logits, std::string* error)
{
    try {
        if (state->n_past + n > state->capacity) { *error = "Context exhausted"; return false; }
        const int E = kE4bEmbd;
        const int p0 = state->n_past;
        const int T = p0 + n;
        const mx::array ids(tokens, {n}, mx::int32);
        mx::array x = EmbedQ6K(m, m->token_embd, ids, E) * std::sqrt(static_cast<float>(E));
        mx::array pt = EmbedQ6K(m, m->per_layer_token_embd, ids, kE4bPleDim * kE4bLayers) * std::sqrt(static_cast<float>(kE4bPleDim));
        mx::array pp = mx::matmul(x, mx::transpose(m->per_layer_model_proj)) * (1.0f / std::sqrt(static_cast<float>(E)));
        pp = Rms(mx::reshape(pp, {n, kE4bLayers, kE4bPleDim}), &m->per_layer_proj_norm);
        const mx::array ple = (pp + mx::reshape(pt, {n, kE4bLayers, kE4bPleDim})) * (1.0f / std::sqrt(2.0f));

        const mx::array query_pos = mx::reshape(mx::arange(p0, T, mx::int32), {n, 1});
        std::vector<mx::array> outputs;
        for (int l = 0; l < kE4bLayers; ++l) {
            Layer& L = m->layers[l];
            const int hd = L.head_dim;
            const auto rope = [&](const mx::array& t) {
                return L.swa ? mx::fast::rope(t, hd, false, kE4bRopeBaseSwa, 1.0f, p0)
                             : mx::fast::rope(t, hd, false, std::nullopt, 1.0f, p0, L.rope_freqs);
            };
            mx::array h = Rms(x, &L.attn_norm);
            mx::array q = Rms(mx::reshape(Linear(m, L.wq, h), {1, n, kE4bHeads, hd}), &L.q_norm);
            q = rope(mx::transpose(q, {0, 2, 1, 3}));
            if (L.owns_kv) {
                mx::array k = Rms(mx::reshape(Linear(m, L.wk, h), {1, n, kE4bKvHeads, hd}), &L.k_norm);
                k = rope(mx::transpose(k, {0, 2, 1, 3}));
                mx::array v = Rms(mx::reshape(Linear(m, L.wv, h), {1, n, kE4bKvHeads, hd}), nullptr);
                v = mx::transpose(v, {0, 2, 1, 3});
                state->k[l] = mx::slice_update(state->k[l], mx::astype(k, mx::float16), {0, 0, p0, 0}, {1, kE4bKvHeads, T, hd});
                state->v[l] = mx::slice_update(state->v[l], mx::astype(v, mx::float16), {0, 0, p0, 0}, {1, kE4bKvHeads, T, hd});
            }
            // Keys the first query of this chunk can see; later queries are masked.
            const int key_start = L.swa ? std::max(0, p0 - kE4bSlidingWindow + 1) : 0;
            const mx::array K = mx::astype(mx::slice(state->k[L.kv_source], {0, 0, key_start, 0}, {1, kE4bKvHeads, T, hd}), mx::float32);
            const mx::array V = mx::astype(mx::slice(state->v[L.kv_source], {0, 0, key_start, 0}, {1, kE4bKvHeads, T, hd}), mx::float32);
            const mx::array qg = mx::reshape(q, {1, kE4bKvHeads, (kE4bHeads / kE4bKvHeads) * n, hd});
            mx::array scores = mx::reshape(mx::matmul(qg, mx::transpose(K, {0, 1, 3, 2})),
                {1, kE4bKvHeads, kE4bHeads / kE4bKvHeads, n, T - key_start});
            if (n > 1 || L.swa) {
                const mx::array key_pos = mx::reshape(mx::arange(key_start, T, mx::int32), {1, T - key_start});
                mx::array allowed = mx::less_equal(key_pos, query_pos);
                if (L.swa) allowed = mx::logical_and(allowed, mx::less(query_pos - key_pos, mx::array(kE4bSlidingWindow)));
                scores = mx::where(allowed, scores, mx::array(-INFINITY, mx::float32));
            }
            const mx::array probs = mx::softmax(scores, -1, true);
            mx::array o = mx::matmul(mx::reshape(probs, {1, kE4bKvHeads, (kE4bHeads / kE4bKvHeads) * n, T - key_start}), V);
            o = mx::reshape(mx::transpose(mx::reshape(o, {1, kE4bHeads, n, hd}), {0, 2, 1, 3}), {n, kE4bHeads * hd});
            x = x + Rms(Linear(m, L.wo, o), &L.post_attention_norm);

            const mx::array f = Rms(x, &L.ffn_norm);
            const mx::array act = GeluTanh(Linear(m, L.gate, f)) * Linear(m, L.up, f);
            x = x + Rms(Linear(m, L.down, act), &L.post_ffw_norm);

            const mx::array ple_l = mx::reshape(mx::slice(ple, {0, l, 0}, {n, l + 1, kE4bPleDim}), {n, kE4bPleDim});
            const mx::array gated = GeluTanh(Linear(m, L.inp_gate, x)) * ple_l;
            x = (x + Rms(Linear(m, L.proj, gated), &L.post_norm)) * L.output_scale;
        }
        mx::array last = Rms(mx::slice(x, {n - 1, 0}, {n, E}), &m->output_norm);
        QWeight head;
        head.bytes = m->token_embd;
        head.q6 = true;
        head.rows = kE4bVocab;
        head.cols = E;
        mx::array out = Gemv(m, head, last);
        out = kE4bLogitSoftcap * mx::tanh(out * (1.0f / kE4bLogitSoftcap));
        outputs.push_back(out);
        for (int l = 0; l < kE4bKvLayers; ++l) { outputs.push_back(state->k[l]); outputs.push_back(state->v[l]); }
        mx::eval(outputs);
        logits->resize(kE4bVocab);
        memcpy(logits->data(), outputs[0].data<float>(), sizeof(float) * kE4bVocab);
        state->n_past = T;
        return true;
    } catch (const std::exception& e) {
        *error = std::string("MLX forward failed: ") + e.what();
        return false;
    }
}

}  // namespace liminal::engine
