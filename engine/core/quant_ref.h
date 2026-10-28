#pragma once
// Scalar reference dequantization (ported from ggml-quants.c). It is the CPU
// oracle the GPU kernels are checked against, not a fast path.
#include "core/e4b_spec.h"

#include <cstdint>

namespace liminal::engine {

float HalfToFloat(uint16_t value);
float Bf16ToFloat(uint16_t value);

// Dequantizes `count` elements of one row (count must be a multiple of 256 for K-quants).
void DequantizeRow(uint32_t type, const uint8_t* row, float* out, int64_t count);
// Dequantizes row `row` of a weight into out[weight.cols].
void DequantizeWeightRow(const WeightRef& weight, int64_t row, float* out);
// out[rows] = W . x for x[cols], f32 accumulation per row.
void MatVecRef(const WeightRef& weight, const float* x, float* out);

}  // namespace liminal::engine
