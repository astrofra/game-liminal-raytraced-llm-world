#include "mlx/mlx_backend.h"

#include "core/gguf.h"

#include <mlx/mlx.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace liminal::engine {

namespace mx = mlx::core;

namespace {

// Metal helpers shared by the K-quant kernels. Block layouts are ggml's:
// Q4_K = f16 d, f16 dmin, scales[12], qs[128] (144 bytes / 256 values)
// Q6_K = ql[128], qh[64], int8 scales[16], f16 d (210 bytes / 256 values)
const char* kHeader = R"METAL(
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

)METAL";

// out[N] = W . x for one f32 activation row; ports of ggml-metal's
// kernel_mul_mv_q4_K_f32 / kernel_mul_mv_q6_K_f32 (MIT). Each simdgroup owns
// NR0 = 2 consecutive rows; the host guarantees rows % 4 == 0.
const char* kGemvQ4K = R"METAL(
	constexpr uint NR0 = 2;
	constexpr uint16_t kmask1 = 0x3f3f;
	constexpr uint16_t kmask2 = 0x0f0f;
	constexpr uint16_t kmask3 = 0xc0c0;
	const uint lane = thread_index_in_simdgroup;
	const uint sg = simdgroup_index_in_threadgroup;
	const uint nsg = threads_per_threadgroup.x / 32;
	const uint row_bytes = uint(w_shape[1]);
	const uint nb = row_bytes / 144;
	const uint first_row = (threadgroup_position_in_grid.x * nsg + sg) * NR0;
	const short ix = lane / 8;
	const short it = lane % 8;
	const short iq = it / 4;
	const short ir = it % 4;
	float yl[16];
	float yh[16];
	float sumf[NR0] = {0.0f, 0.0f};
	device const float* y4 = x + ix * 256 + 64 * iq + 8 * ir;
	uint16_t sc16[4];
	thread const uint8_t* sc8 = (thread const uint8_t*)sc16;
	for (uint ib = ix; ib < nb; ib += 4) {
		float4 sumy = {0.0f, 0.0f, 0.0f, 0.0f};
		for (short i = 0; i < 8; ++i) {
			yl[i + 0] = y4[i + 0]; sumy[0] += yl[i + 0];
			yl[i + 8] = y4[i + 32]; sumy[1] += yl[i + 8];
			yh[i + 0] = y4[i + 128]; sumy[2] += yh[i + 0];
			yh[i + 8] = y4[i + 160]; sumy[3] += yh[i + 8];
		}
		device const uint8_t* block = w + first_row * row_bytes + ib * 144;
		device const uint16_t* sc = (device const uint16_t*)(block + 4) + iq;
		device const uint16_t* q1 = (device const uint16_t*)(block + 16) + 16 * iq + 4 * ir;
		device const half* dh = (device const half*)block;
		for (uint row = 0; row < NR0; ++row) {
			sc16[0] = sc[0] & kmask1;
			sc16[1] = sc[2] & kmask1;
			sc16[2] = ((sc[4] >> 0) & kmask2) | ((sc[0] & kmask3) >> 2);
			sc16[3] = ((sc[4] >> 4) & kmask2) | ((sc[2] & kmask3) >> 2);
			device const uint16_t* q2 = q1 + 32;
			float4 acc1 = {0.0f, 0.0f, 0.0f, 0.0f};
			float4 acc2 = {0.0f, 0.0f, 0.0f, 0.0f};
			for (short i = 0; i < 4; ++i) {
				acc1[0] += yl[2 * i + 0] * (q1[i] & 0x000F);
				acc1[1] += yl[2 * i + 1] * (q1[i] & 0x0F00);
				acc1[2] += yl[2 * i + 8] * (q1[i] & 0x00F0);
				acc1[3] += yl[2 * i + 9] * (q1[i] & 0xF000);
				acc2[0] += yh[2 * i + 0] * (q2[i] & 0x000F);
				acc2[1] += yh[2 * i + 1] * (q2[i] & 0x0F00);
				acc2[2] += yh[2 * i + 8] * (q2[i] & 0x00F0);
				acc2[3] += yh[2 * i + 9] * (q2[i] & 0xF000);
			}
			sumf[row] += float(dh[0]) * ((acc1[0] + 1.f / 256.f * acc1[1]) * sc8[0] +
			                             (acc1[2] + 1.f / 256.f * acc1[3]) * sc8[1] * 1.f / 16.f +
			                             (acc2[0] + 1.f / 256.f * acc2[1]) * sc8[4] +
			                             (acc2[2] + 1.f / 256.f * acc2[3]) * sc8[5] * 1.f / 16.f) -
			             float(dh[1]) * (sumy[0] * sc8[2] + sumy[1] * sc8[3] + sumy[2] * sc8[6] + sumy[3] * sc8[7]);
			q1 += row_bytes / 2;
			sc += row_bytes / 2;
			dh += row_bytes / 2;
		}
		y4 += 4 * 256;
	}
	for (uint row = 0; row < NR0; ++row) {
		const float total = simd_sum(sumf[row]);
		if (lane == 0) out[first_row + row] = total;
	}
)METAL";

const char* kGemvQ6K = R"METAL(
	constexpr uint NR0 = 2;
	constexpr uint8_t kmask1 = 0x03;
	constexpr uint8_t kmask2 = 0x0C;
	constexpr uint8_t kmask3 = 0x30;
	constexpr uint8_t kmask4 = 0xC0;
	const uint lane = thread_index_in_simdgroup;
	const uint sg = simdgroup_index_in_threadgroup;
	const uint nsg = threads_per_threadgroup.x / 32;
	const uint row_bytes = uint(w_shape[1]);
	const uint nb = row_bytes / 210;
	const uint first_row = (threadgroup_position_in_grid.x * nsg + sg) * NR0;
	float sumf[NR0] = {0.0f, 0.0f};
	float yl[16];
	const short tid = lane / 2;
	const short ix = lane % 2;
	const short ip = tid / 8;
	const short il = tid % 8;
	const short l0 = 4 * il;
	const short is = 8 * ip + l0 / 16;
	const short y_offset = 128 * ip + l0;
	const short q_offset_l = 64 * ip + l0;
	const short q_offset_h = 32 * ip + l0;
	for (uint i = ix; i < nb; i += 2) {
		device const uint8_t* block = w + first_row * row_bytes + i * 210;
		device const uint8_t* q1 = block + q_offset_l;
		device const uint8_t* q2 = q1 + 32;
		device const uint8_t* qh = block + 128 + q_offset_h;
		device const int8_t* sc = (device const int8_t*)(block + 192) + is;
		device const half* dh = (device const half*)(block + 208);
		device const float* y = x + i * 256 + y_offset;
		for (short l = 0; l < 4; ++l) {
			yl[4 * l + 0] = y[l + 0];
			yl[4 * l + 1] = y[l + 32];
			yl[4 * l + 2] = y[l + 64];
			yl[4 * l + 3] = y[l + 96];
		}
		for (uint row = 0; row < NR0; ++row) {
			float4 sums = {0.0f, 0.0f, 0.0f, 0.0f};
			for (short l = 0; l < 4; ++l) {
				sums[0] += yl[4 * l + 0] * ((int8_t)((q1[l] & 0xF) | ((qh[l] & kmask1) << 4)) - 32);
				sums[1] += yl[4 * l + 1] * ((int8_t)((q2[l] & 0xF) | ((qh[l] & kmask2) << 2)) - 32);
				sums[2] += yl[4 * l + 2] * ((int8_t)((q1[l] >> 4) | ((qh[l] & kmask3) << 0)) - 32);
				sums[3] += yl[4 * l + 3] * ((int8_t)((q2[l] >> 4) | ((qh[l] & kmask4) >> 2)) - 32);
			}
			sumf[row] += float(dh[0]) * (sums[0] * sc[0] + sums[1] * sc[2] + sums[2] * sc[4] + sums[3] * sc[6]);
			q1 += row_bytes;
			q2 += row_bytes;
			qh += row_bytes;
			sc += row_bytes;
			dh += row_bytes / 2;
		}
	}
	for (uint row = 0; row < NR0; ++row) {
		const float total = simd_sum(sumf[row]);
		if (lane == 0) out[first_row + row] = total;
	}
)METAL";

// Prefill path: whole K-quant matrices are expanded to f16 (one thread per
// 32-value subblock) so MLX's own GEMM does the multiply.
const char* kDequantQ4K = R"METAL(
	const uint index = thread_position_in_grid.x;
	const uint row_bytes = uint(w_shape[1]);
	const uint cols = row_bytes / 144 * 256;
	const uint row = index / (cols / 32);
	const uint k_base = (index % (cols / 32)) * 32;
	if (row >= uint(w_shape[0])) return;
	device const uint8_t* block = w + row * row_bytes + (k_base / 256) * 144;
	const uint subblock = (k_base % 256) / 32;
	device const uint8_t* scales = block + 4;
	uint scale, minimum;
	if (subblock < 4) { scale = scales[subblock] & 63; minimum = scales[subblock + 4] & 63; }
	else {
		scale = (scales[subblock + 4] & 15) | ((scales[subblock - 4] >> 6) << 4);
		minimum = (scales[subblock + 4] >> 4) | ((scales[subblock] >> 6) << 4);
	}
	const float d = half_from_bytes(block) * float(scale);
	const float m = half_from_bytes(block + 2) * float(minimum);
	device const uint8_t* packed = block + 16 + (subblock / 2) * 32;
	const uint shift = (subblock & 1) * 4;
	device half* dst = out + row * cols + k_base;
	for (uint i = 0; i < 32; ++i) dst[i] = half(d * float((packed[i] >> shift) & 15) - m);
)METAL";

const char* kDequantQ6K = R"METAL(
	const uint index = thread_position_in_grid.x;
	const uint row_bytes = uint(w_shape[1]);
	const uint cols = row_bytes / 210 * 256;
	const uint row = index / (cols / 32);
	const uint k_base = (index % (cols / 32)) * 32;
	if (row >= uint(w_shape[0])) return;
	device const uint8_t* block = w + row * row_bytes + (k_base / 256) * 210;
	const uint subblock = (k_base % 256) / 32;
	const uint cluster = subblock / 4;
	const uint cluster_subblock = subblock % 4;
	device const uint8_t* low = block + cluster * 64;
	device const uint8_t* high = block + 128 + cluster * 32;
	device const int8_t* scales = reinterpret_cast<device const int8_t*>(block + 192);
	const float delta = half_from_bytes(block + 208);
	device half* dst = out + row * cols + k_base;
	for (uint i = 0; i < 32; ++i) {
		const uint cluster_index = cluster_subblock * 32 + i;
		const uint low_bits = (low[cluster_index % 64] >> ((cluster_index / 64) * 4)) & 15;
		const uint high_bits = ((high[i] >> (cluster_subblock * 2)) & 3) << 4;
		dst[i] = half(delta * float(scales[subblock * 2 + i / 16] * (int(low_bits | high_bits) - 32)));
	}
)METAL";

// Decode attention, split over the key axis (flash decoding). Threadgroup
// (split, kv head) serves the 4 query heads sharing that KV head: simdgroup sg
// runs an online softmax for head sg % 4 over every other key of the split,
// so the 4 heads read each cached row together through L1. Lane l holds
// elements l, l + 32, ... of the head. The f16 caches are read in place up to
// pos and the sliding window is applied here. Attention scale is 1.
const char* kAttnDecodePartial = R"METAL(
	constexpr uint E = HD / 32;
	constexpr uint G = 4;
	const uint split = threadgroup_position_in_grid.x;
	const uint kv = threadgroup_position_in_grid.y;
	const uint n_splits = threadgroups_per_grid.x;
	const uint lane = thread_index_in_simdgroup;
	const uint sg = simdgroup_index_in_threadgroup;
	const uint tid = thread_index_in_threadgroup;
	const uint g = sg % G;
	const uint stream = sg / G;
	const int cap = int(k_shape[2]);
	const int p = pos[0];
	// The grid covers only the splits overlapping the visible keys.
	const int first_key = WINDOW > 0 ? max(0, p - WINDOW + 1) : 0;
	int begin = (first_key / SPLIT + int(split)) * SPLIT;
	const int end = min(begin + SPLIT, p + 1);
	begin = max(begin, first_key);
	device const half* kb = k + kv * uint(cap) * HD;
	device const half* vb = v + kv * uint(cap) * HD;
	const uint head = kv * G + g;
	float qv[E];
	float acc[E];
	for (uint i = 0; i < E; ++i) { qv[i] = q[head * HD + i * 32 + lane]; acc[i] = 0.0f; }
	float m = -INFINITY;
	float l = 0.0f;
	for (int t = begin + int(stream); t < end; t += 2) {
		device const half* kr = kb + uint(t) * HD;
		float dot = 0.0f;
		for (uint i = 0; i < E; ++i) dot += qv[i] * float(kr[i * 32 + lane]);
		const float score = simd_sum(dot);
		const float mn = max(m, score);
		const float c = exp(m - mn);
		const float w = exp(score - mn);
		l = l * c + w;
		device const half* vr = vb + uint(t) * HD;
		for (uint i = 0; i < E; ++i) acc[i] = acc[i] * c + w * float(vr[i * 32 + lane]);
		m = mn;
	}
	threadgroup float tm[8];
	threadgroup float tl[8];
	threadgroup float tacc[8 * HD];
	if (lane == 0) { tm[sg] = m; tl[sg] = l; }
	for (uint i = 0; i < E; ++i) tacc[sg * HD + i * 32 + lane] = acc[i];
	threadgroup_barrier(mem_flags::mem_threadgroup);
	// Thread tid combines the two streams of head tid / (256 / G).
	const uint gh = tid / (256 / G);
	const uint first = tid % (256 / G);
	const float m0 = tm[gh];
	const float m1 = tm[G + gh];
	const float M = max(m0, m1);
	const float w0 = m0 == -INFINITY ? 0.0f : exp(m0 - M);
	const float w1 = m1 == -INFINITY ? 0.0f : exp(m1 - M);
	const uint slot = (kv * G + gh) * n_splits + split;
	for (uint d = first; d < HD; d += 256 / G) partial_o[slot * HD + d] = tacc[gh * HD + d] * w0 + tacc[(G + gh) * HD + d] * w1;
	if (first == 0) {
		partial_ml[slot * 2] = M;
		partial_ml[slot * 2 + 1] = tl[gh] * w0 + tl[G + gh] * w1;
	}
)METAL";

const char* kAttnDecodeReduce = R"METAL(
	const uint d = thread_position_in_grid.x;
	const uint head = thread_position_in_grid.y;
	const uint n_splits = uint(partial_ml_shape[1]);
	float M = -INFINITY;
	for (uint s = 0; s < n_splits; ++s) M = max(M, partial_ml[(head * n_splits + s) * 2]);
	float L = 0.0f;
	float o = 0.0f;
	for (uint s = 0; s < n_splits; ++s) {
		const float ms = partial_ml[(head * n_splits + s) * 2];
		if (ms == -INFINITY) continue;
		const float w = exp(ms - M);
		L += partial_ml[(head * n_splits + s) * 2 + 1] * w;
		o += partial_o[(head * n_splits + s) * HD + d] * w;
	}
	out[head * HD + d] = o / L;
)METAL";

// Sum of a per-thread value over a 1024-thread threadgroup.
const char* kReduceHeader = R"METAL(
METAL_FUNC float threadgroup_sum(float value, threadgroup float* scratch, uint tid, uint threads) {
	value = simd_sum(value);
	if (tid % 32 == 0) scratch[tid / 32] = value;
	threadgroup_barrier(mem_flags::mem_threadgroup);
	float total = 0.0f;
	for (uint i = 0; i < threads / 32; ++i) total += scratch[i];
	threadgroup_barrier(mem_flags::mem_threadgroup);
	return total;
}
)METAL";

// Per-head RMS norm (weighted for Q/K, plain for V) then NEOX RoPE, one
// threadgroup per (head slot, token). Q leaves as f32 [8, n, hd]; K/V leave as
// f16 [2, n, hd], ready for the cache. Slots: 0-7 Q, 8-9 K, 10-11 V.
const char* kQkvPrep = R"METAL(
	constexpr uint P = HD / 2;
	const uint slot = threadgroup_position_in_grid.x;
	const uint t = threadgroup_position_in_grid.y;
	const uint n = threadgroups_per_grid.y;
	const uint tid = thread_index_in_threadgroup;
	threadgroup float scratch[32];
	const uint kind = slot < 8 ? 0 : slot < 10 ? 1 : 2;
	const uint h = kind == 0 ? slot : kind == 1 ? slot - 8 : slot - 10;
#if HAS_KV
	device const float* src = kind == 0 ? q + (t * 8 + h) * HD : kind == 1 ? k + (t * 2 + h) * HD : v + (t * 2 + h) * HD;
#else
	device const float* src = q + (t * 8 + h) * HD;
#endif
	float a[2], b[2];
	float ss = 0.0f;
	for (uint j = 0; j < 2; ++j) {
		const uint i = tid + j * 256;
		a[j] = i < P ? src[i] : 0.0f;
		b[j] = i < P ? src[i + P] : 0.0f;
		ss += a[j] * a[j] + b[j] * b[j];
	}
	const float inv = metal::precise::rsqrt(threadgroup_sum(ss, scratch, tid, 256) / float(HD) + 1e-6f);
	const float position = float(pos[0] + int(t));
	for (uint j = 0; j < 2; ++j) {
		const uint i = tid + j * 256;
		if (i >= P) continue;
		float x0 = a[j] * inv;
		float x1 = b[j] * inv;
		if (kind == 2) {
#if HAS_KV
			vo[(h * n + t) * HD + i] = half(x0);
			vo[(h * n + t) * HD + i + P] = half(x1);
#endif
			continue;
		}
#if HAS_KV
		device const float* w = kind == 0 ? qw : kw;
#else
		device const float* w = qw;
#endif
		x0 *= w[i];
		x1 *= w[i + P];
		const float theta = position * inv_freq[i];
		const float c = metal::precise::cos(theta);
		const float s = metal::precise::sin(theta);
		const float r0 = x0 * c - x1 * s;
		const float r1 = x0 * s + x1 * c;
		if (kind == 0) {
			qo[(h * n + t) * HD + i] = r0;
			qo[(h * n + t) * HD + i + P] = r1;
		}
#if HAS_KV
		else {
			ko[(h * n + t) * HD + i] = half(r0);
			ko[(h * n + t) * HD + i + P] = half(r1);
		}
#endif
	}
)METAL";

// x' = (x + rms(y) * w) * scale and, with HAS_NEXT, h = rms(x') * wn: the
// residual update of each block fused with the next block's input norm. One
// 1024-thread threadgroup per row.
const char* kAddNorm = R"METAL(
	constexpr uint E = 2560;
	const uint row = threadgroup_position_in_grid.x;
	const uint tid = thread_index_in_threadgroup;
	threadgroup float scratch[32];
	float yv[3], xv[3];
	float ss = 0.0f;
	for (uint j = 0; j < 3; ++j) {
		const uint i = tid + j * 1024;
		yv[j] = i < E ? y[row * E + i] : 0.0f;
		ss += yv[j] * yv[j];
	}
	const float inv = metal::precise::rsqrt(threadgroup_sum(ss, scratch, tid, 1024) / float(E) + 1e-6f);
	float ss2 = 0.0f;
	for (uint j = 0; j < 3; ++j) {
		const uint i = tid + j * 1024;
		if (i >= E) continue;
		xv[j] = (x[row * E + i] + yv[j] * inv * w[i]) * scale[0];
		xo[row * E + i] = xv[j];
		ss2 += xv[j] * xv[j];
	}
#if HAS_NEXT
	const float inv2 = metal::precise::rsqrt(threadgroup_sum(ss2, scratch, tid, 1024) / float(E) + 1e-6f);
	for (uint j = 0; j < 3; ++j) {
		const uint i = tid + j * 1024;
		if (i < E) ho[row * E + i] = xv[j] * inv2 * wn[i];
	}
#endif
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
    mx::fast::CustomKernelFunction dequant_q4k = mx::fast::metal_kernel("liminal_dequant_q4k", {"w"}, {"out"}, kDequantQ4K, kHeader);
    mx::fast::CustomKernelFunction dequant_q6k = mx::fast::metal_kernel("liminal_dequant_q6k", {"w"}, {"out"}, kDequantQ6K, kHeader);
    mx::fast::CustomKernelFunction attn_partial = mx::fast::metal_kernel("liminal_attn_decode_partial", {"q", "k", "v", "pos"},
        {"partial_o", "partial_ml"}, kAttnDecodePartial, kHeader);
    mx::fast::CustomKernelFunction attn_reduce = mx::fast::metal_kernel("liminal_attn_decode_reduce", {"partial_o", "partial_ml"},
        {"out"}, kAttnDecodeReduce, kHeader);
    mx::fast::CustomKernelFunction prep_qkv = mx::fast::metal_kernel("liminal_prep_qkv",
        {"q", "k", "v", "qw", "kw", "inv_freq", "pos"}, {"qo", "ko", "vo"}, std::string("#define HAS_KV 1\n") + kQkvPrep,
        std::string(kHeader) + kReduceHeader);
    mx::fast::CustomKernelFunction prep_q = mx::fast::metal_kernel("liminal_prep_q", {"q", "qw", "inv_freq", "pos"}, {"qo"},
        std::string("#define HAS_KV 0\n") + kQkvPrep, std::string(kHeader) + kReduceHeader);
    mx::fast::CustomKernelFunction add_norm = mx::fast::metal_kernel("liminal_add_norm", {"x", "y", "w", "scale"}, {"xo"},
        std::string("#define HAS_NEXT 0\n") + kAddNorm, std::string(kHeader) + kReduceHeader);
    mx::fast::CustomKernelFunction add_norm_next = mx::fast::metal_kernel("liminal_add_norm_next",
        {"x", "y", "w", "wn", "scale"}, {"xo", "ho"}, std::string("#define HAS_NEXT 1\n") + kAddNorm,
        std::string(kHeader) + kReduceHeader);
    mx::fast::CustomKernelFunction embed_q6k = mx::fast::metal_kernel("liminal_embed_q6k", {"w", "ids"}, {"out"}, kEmbedQ6K, kHeader);
};

struct Layer {
    bool swa = false;
    int head_dim = 0;
    bool owns_kv = false;
    int kv_source = 0;
    mx::array output_scale = mx::array(0);  // [1]
    mx::array inv_freq = mx::array(0);  // [head_dim / 2] RoPE inverse frequencies
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
    if (q.rows % 4 != 0) throw std::runtime_error("GEMV needs a multiple of 4 rows");
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
    mx::array one = mx::ones({1}, mx::float32);
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
    const int rows_per_group = 2 * sg_count;
    const int groups = (w.rows + rows_per_group - 1) / rows_per_group;
    auto out = (w.q6 ? m->kernels.gemv_q6k : m->kernels.gemv_q4k)(
        {mx::reshape(mx::astype(x, mx::float32), {w.cols}), w.bytes}, {{w.rows}}, {mx::float32},
        {groups * 32 * sg_count, 1, 1}, {32 * sg_count, 1, 1}, {}, std::nullopt, false, mx::Device::gpu);
    return mx::reshape(out[0], {1, w.rows});
}

static mx::array Dequantize(MlxModel* m, const QWeight& w)
{
    const int threads = w.rows * (w.cols / 32);
    auto out = (w.q6 ? m->kernels.dequant_q6k : m->kernels.dequant_q4k)(
        {w.bytes}, {{w.rows, w.cols}}, {mx::float16}, {threads, 1, 1}, {256, 1, 1}, {}, std::nullopt, false, mx::Device::gpu);
    return out[0];
}

// x: [n, cols] -> [n, rows]. Prefill expands the weights to f16 for MLX's GEMM
// (f16 operands as llama.cpp's mul_mm; MLX rounds the output to f16).
static mx::array Linear(MlxModel* m, const QWeight& w, const mx::array& x)
{
    if (x.shape(0) == 1) return Gemv(m, w, x);
    return mx::astype(mx::matmul(mx::astype(x, mx::float16), mx::transpose(Dequantize(m, w))), mx::float32);
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

// Elementwise chains fused into single kernels by mx::compile.
static mx::array GeGlu(const mx::array& gate, const mx::array& value)
{
    static auto fused = mx::compile([](const std::vector<mx::array>& in) {
        return std::vector<mx::array>{GeluTanh(in[0]) * in[1]};
    }, true);
    return fused({gate, value})[0];
}

// q: [8, hd] f32 after RoPE; caches [1, 2, capacity, hd] f16 hold position pos.
static mx::array AttendDecode(MlxModel* m, const mx::array& q, const mx::array& k, const mx::array& v, const mx::array& pos,
    int position, int head_dim, bool swa)
{
    constexpr int kSplit = 64;
    const int first_key = swa ? std::max(0, position - kE4bSlidingWindow + 1) : 0;
    const int splits = position / kSplit - first_key / kSplit + 1;
    auto partial = m->kernels.attn_partial({q, k, v, pos}, {{kE4bHeads * splits * head_dim}, {kE4bHeads, splits, 2}},
        {mx::float32, mx::float32}, {splits * 256, kE4bKvHeads, 1}, {256, 1, 1},
        {{"HD", head_dim}, {"SPLIT", kSplit}, {"WINDOW", swa ? kE4bSlidingWindow : 0}}, std::nullopt, false, mx::Device::gpu);
    auto out = m->kernels.attn_reduce({partial[0], partial[1]}, {{1, kE4bHeads * head_dim}}, {mx::float32},
        {head_dim, kE4bHeads, 1}, {std::min(head_dim, 256), 1, 1}, {{"HD", head_dim}}, std::nullopt, false, mx::Device::gpu);
    return out[0];
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
            dst.output_scale = mx::array(&src.output_scale, {1}, mx::float32);
            // Full layers scale each frequency by rope_freqs (64 rotating pairs out of 256).
            std::vector<float> inv_freq(static_cast<size_t>(src.head_dim / 2));
            for (size_t i = 0; i < inv_freq.size(); ++i) {
                const double exponent = 2.0 * static_cast<double>(i) / static_cast<double>(src.head_dim);
                inv_freq[i] = static_cast<float>(1.0 / (std::pow(static_cast<double>(src.rope_base), exponent) *
                    (src.swa ? 1.0 : static_cast<double>(model.rope_freq_factors[i]))));
            }
            dst.inv_freq = mx::array(inv_freq.data(), {static_cast<int>(inv_freq.size())}, mx::float32);
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
        const auto profile_start = std::chrono::steady_clock::now();
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
        const mx::array pos = mx::array(&p0, {1}, mx::int32);
        const auto gpu = mx::Device::gpu;
        // h is the current block's normalized input; add_norm_next produces it
        // together with the residual update.
        mx::array h = Rms(x, &m->layers[0].attn_norm);
        for (int l = 0; l < kE4bLayers; ++l) {
            Layer& L = m->layers[l];
            const int hd = L.head_dim;
            mx::array q = mx::array(0);
            if (L.owns_kv) {
                auto prep = m->kernels.prep_qkv({Linear(m, L.wq, h), Linear(m, L.wk, h), Linear(m, L.wv, h), L.q_norm, L.k_norm, L.inv_freq, pos},
                    {{kE4bHeads, n, hd}, {1, kE4bKvHeads, n, hd}, {1, kE4bKvHeads, n, hd}}, {mx::float32, mx::float16, mx::float16},
                    {(kE4bHeads + 2 * kE4bKvHeads) * 256, n, 1}, {256, 1, 1}, {{"HD", hd}}, std::nullopt, false, gpu);
                q = prep[0];
                state->k[l] = mx::slice_update(state->k[l], prep[1], {0, 0, p0, 0}, {1, kE4bKvHeads, T, hd});
                state->v[l] = mx::slice_update(state->v[l], prep[2], {0, 0, p0, 0}, {1, kE4bKvHeads, T, hd});
            } else {
                q = m->kernels.prep_q({Linear(m, L.wq, h), L.q_norm, L.inv_freq, pos}, {{kE4bHeads, n, hd}}, {mx::float32},
                    {kE4bHeads * 256, n, 1}, {256, 1, 1}, {{"HD", hd}}, std::nullopt, false, gpu)[0];
            }
            mx::array o = mx::array(0);
            if (n == 1) {
                o = AttendDecode(m, mx::reshape(q, {kE4bHeads, hd}), state->k[L.kv_source], state->v[L.kv_source], pos, p0, hd, L.swa);
            } else {
                // Keys the first query of this chunk can see; later queries are masked.
                const int key_start = L.swa ? std::max(0, p0 - kE4bSlidingWindow + 1) : 0;
                const mx::array K = mx::astype(mx::slice(state->k[L.kv_source], {0, 0, key_start, 0}, {1, kE4bKvHeads, T, hd}), mx::float32);
                const mx::array V = mx::astype(mx::slice(state->v[L.kv_source], {0, 0, key_start, 0}, {1, kE4bKvHeads, T, hd}), mx::float32);
                const mx::array qg = mx::reshape(q, {1, kE4bKvHeads, (kE4bHeads / kE4bKvHeads) * n, hd});
                mx::array scores = mx::reshape(mx::matmul(qg, mx::transpose(K, {0, 1, 3, 2})),
                    {1, kE4bKvHeads, kE4bHeads / kE4bKvHeads, n, T - key_start});
                const mx::array key_pos = mx::reshape(mx::arange(key_start, T, mx::int32), {1, T - key_start});
                mx::array allowed = mx::less_equal(key_pos, query_pos);
                if (L.swa) allowed = mx::logical_and(allowed, mx::less(query_pos - key_pos, mx::array(kE4bSlidingWindow)));
                scores = mx::where(allowed, scores, mx::array(-INFINITY, mx::float32));
                const mx::array probs = mx::softmax(scores, -1, true);
                o = mx::matmul(mx::reshape(probs, {1, kE4bKvHeads, (kE4bHeads / kE4bKvHeads) * n, T - key_start}), V);
                o = mx::reshape(mx::transpose(mx::reshape(o, {1, kE4bHeads, n, hd}), {0, 2, 1, 3}), {n, kE4bHeads * hd});
            }
            const std::tuple<int, int, int> row_grid = {n * 1024, 1, 1};
            const std::tuple<int, int, int> row_group = {1024, 1, 1};
            auto attn = m->kernels.add_norm_next({x, Linear(m, L.wo, o), L.post_attention_norm, L.ffn_norm, m->one},
                {{n, E}, {n, E}}, {mx::float32, mx::float32}, row_grid, row_group, {}, std::nullopt, false, gpu);
            x = attn[0];
            const mx::array act = GeGlu(Linear(m, L.gate, attn[1]), Linear(m, L.up, attn[1]));
            x = m->kernels.add_norm({x, Linear(m, L.down, act), L.post_ffw_norm, m->one}, {{n, E}}, {mx::float32},
                row_grid, row_group, {}, std::nullopt, false, gpu)[0];

            const mx::array ple_l = mx::reshape(mx::slice(ple, {0, l, 0}, {n, l + 1, kE4bPleDim}), {n, kE4bPleDim});
            const mx::array gated = GeGlu(Linear(m, L.inp_gate, x), ple_l);
            const mx::array& next_norm = l + 1 < kE4bLayers ? m->layers[l + 1].attn_norm : m->output_norm;
            auto end = m->kernels.add_norm_next({x, Linear(m, L.proj, gated), L.post_norm, next_norm, L.output_scale},
                {{n, E}, {n, E}}, {mx::float32, mx::float32}, row_grid, row_group, {}, std::nullopt, false, gpu);
            x = end[0];
            h = end[1];
        }
        // After the last layer h is rms(x) * output_norm.
        const mx::array last = mx::slice(h, {n - 1, 0}, {n, E});
        QWeight head;
        head.bytes = m->token_embd;
        head.q6 = true;
        head.rows = kE4bVocab;
        head.cols = E;
        mx::array out = Gemv(m, head, last);
        out = kE4bLogitSoftcap * mx::tanh(out * (1.0f / kE4bLogitSoftcap));
        std::vector<mx::array> outputs = {out};
        for (int l = 0; l < kE4bKvLayers; ++l) { outputs.push_back(state->k[l]); outputs.push_back(state->v[l]); }
        static const bool profile = getenv("LIMINAL_MLX_PROFILE") != nullptr;
        const auto built = std::chrono::steady_clock::now();
        mx::eval(outputs);
        if (profile && n == 1) {
            static double build_ms = 0, eval_ms = 0;
            static int steps = 0;
            const auto done = std::chrono::steady_clock::now();
            build_ms += std::chrono::duration<double, std::milli>(built - profile_start).count();
            eval_ms += std::chrono::duration<double, std::milli>(done - built).count();
            if (++steps % 32 == 0) fprintf(stderr, "mlx decode: build %.2f ms, eval %.2f ms per token\n", build_ms / steps, eval_ms / steps);
        }
        logits->resize(kE4bVocab);
        memcpy(logits->data(), outputs[0].data<float>(), sizeof(float) * kE4bVocab);
        state->n_past = T;
        return true;
    } catch (const std::exception& e) {
        *error = std::string("MLX forward failed: ") + e.what();
        return false;
    }
}

void MlxBenchGemv(MlxModel* m, int repeats)
{
    struct Group { const char* name; std::vector<const QWeight*> weights; };
    std::vector<Group> groups = {{"attention", {}}, {"ffn", {}}, {"ple", {}}, {"head", {}}};
    QWeight head;
    head.bytes = m->token_embd;
    head.q6 = true;
    head.rows = kE4bVocab;
    head.cols = kE4bEmbd;
    for (Layer& L : m->layers) {
        for (const QWeight* q : {&L.wq, &L.wk, &L.wv, &L.wo}) if (q->rows) groups[0].weights.push_back(q);
        for (const QWeight* q : {&L.gate, &L.up, &L.down}) groups[1].weights.push_back(q);
        for (const QWeight* q : {&L.inp_gate, &L.proj}) groups[2].weights.push_back(q);
    }
    groups[3].weights.push_back(&head);
    groups.push_back({"all", {}});
    for (int g = 0; g < 4; ++g) for (const QWeight* q : groups[g].weights) groups[4].weights.push_back(q);
    for (const Group& group : groups) {
        double bytes = 0;
        for (const QWeight* q : group.weights) bytes += static_cast<double>(q->bytes.nbytes());
        double best = 1e30;
        for (int r = 0; r < repeats; ++r) {
            std::vector<mx::array> outs;
            for (const QWeight* q : group.weights) outs.push_back(Gemv(m, *q, mx::ones({1, q->cols}, mx::float32)));
            const auto start = std::chrono::steady_clock::now();
            mx::eval(outs);
            best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        }
        printf("gemv %-10s %4zu kernels %7.1f MB  %6.2f ms  %6.1f GB/s\n", group.name, group.weights.size(), bytes / 1e6,
            best * 1e3, bytes / best / 1e9);
    }
}

}  // namespace liminal::engine
