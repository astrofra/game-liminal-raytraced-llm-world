#include "core/quant_ref.h"

#include <cstring>
#include <vector>

namespace liminal::engine {

namespace {

static uint16_t Load16(const uint8_t* p)
{
    uint16_t value;
    memcpy(&value, p, 2);
    return value;
}

static void ScaleMinK4(int j, const uint8_t* q, uint8_t* d, uint8_t* m)
{
    if (j < 4) {
        *d = q[j] & 63; *m = q[j + 4] & 63;
    } else {
        *d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4);
        *m = (q[j+4] >>  4) | ((q[j-0] >> 6) << 4);
    }
}

// block_q4_K: f16 d, f16 dmin, scales[12], qs[128]
static void DequantizeQ4K(const uint8_t* block, float* y)
{
    const float d = HalfToFloat(Load16(block));
    const float min = HalfToFloat(Load16(block + 2));
    const uint8_t* scales = block + 4;
    const uint8_t* q = block + 16;
    int is = 0;
    uint8_t sc, m;
    for (int j = 0; j < kQkK; j += 64) {
        ScaleMinK4(is + 0, scales, &sc, &m);
        const float d1 = d * sc; const float m1 = min * m;
        ScaleMinK4(is + 1, scales, &sc, &m);
        const float d2 = d * sc; const float m2 = min * m;
        for (int l = 0; l < 32; ++l) *y++ = d1 * (q[l] & 0xF) - m1;
        for (int l = 0; l < 32; ++l) *y++ = d2 * (q[l]  >> 4) - m2;
        q += 32; is += 2;
    }
}

// block_q6_K: ql[128], qh[64], int8 scales[16], f16 d
static void DequantizeQ6K(const uint8_t* block, float* y)
{
    const uint8_t* ql = block;
    const uint8_t* qh = block + 128;
    const int8_t* sc = reinterpret_cast<const int8_t*>(block + 192);
    const float d = HalfToFloat(Load16(block + 208));
    for (int n = 0; n < kQkK; n += 128) {
        for (int l = 0; l < 32; ++l) {
            const int is = l/16;
            const int8_t q1 = (int8_t)((ql[l +  0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
            const int8_t q2 = (int8_t)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
            const int8_t q3 = (int8_t)((ql[l +  0]  >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
            const int8_t q4 = (int8_t)((ql[l + 32]  >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
            y[l +  0] = d * sc[is + 0] * q1;
            y[l + 32] = d * sc[is + 2] * q2;
            y[l + 64] = d * sc[is + 4] * q3;
            y[l + 96] = d * sc[is + 6] * q4;
        }
        y  += 128;
        ql += 64;
        qh += 32;
        sc += 8;
    }
}

}  // namespace

float HalfToFloat(uint16_t h)
{
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000) << 16;
    uint32_t exponent = (h >> 10) & 0x1f;
    uint32_t mantissa = h & 0x3ff;
    uint32_t bits;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400) == 0) { mantissa <<= 1; --exponent; }
            mantissa &= 0x3ff;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7f800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

float Bf16ToFloat(uint16_t value)
{
    const uint32_t bits = static_cast<uint32_t>(value) << 16;
    float result;
    memcpy(&result, &bits, 4);
    return result;
}

void DequantizeRow(uint32_t type, const uint8_t* row, float* out, int64_t count)
{
    switch (type) {
        case kGgmlF32: memcpy(out, row, static_cast<size_t>(count) * 4); break;
        case kGgmlF16: for (int64_t i = 0; i < count; ++i) out[i] = HalfToFloat(Load16(row + 2 * i)); break;
        case kGgmlBF16: for (int64_t i = 0; i < count; ++i) out[i] = Bf16ToFloat(Load16(row + 2 * i)); break;
        case kGgmlQ4K: for (int64_t b = 0; b < count / kQkK; ++b) DequantizeQ4K(row + b * kQ4KBlockBytes, out + b * kQkK); break;
        case kGgmlQ6K: for (int64_t b = 0; b < count / kQkK; ++b) DequantizeQ6K(row + b * kQ6KBlockBytes, out + b * kQkK); break;
        default: memset(out, 0, static_cast<size_t>(count) * 4); break;
    }
}

void DequantizeWeightRow(const WeightRef& weight, int64_t row, float* out)
{
    DequantizeRow(weight.type, weight.data + row * weight.row_bytes, out, weight.cols);
}

void MatVecRef(const WeightRef& weight, const float* x, float* out)
{
#pragma omp parallel
    {
        std::vector<float> row(static_cast<size_t>(weight.cols));
#pragma omp for schedule(static)
        for (int64_t r = 0; r < weight.rows; ++r) {
            DequantizeWeightRow(weight, r, row.data());
            double sum = 0.0;
            for (int64_t c = 0; c < weight.cols; ++c) sum += static_cast<double>(row[static_cast<size_t>(c)]) * x[c];
            out[r] = static_cast<float>(sum);
        }
    }
}

}  // namespace liminal::engine
