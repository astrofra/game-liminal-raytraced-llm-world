#pragma once
// Read-only GGUF v3 view over a memory-mapped file. Every read is bounds-checked:
// a truncated or malformed file yields an error string, never a crash.
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace liminal::engine {

enum GgufValueType : uint32_t {
    kGgufUint8 = 0, kGgufInt8 = 1, kGgufUint16 = 2, kGgufInt16 = 3, kGgufUint32 = 4, kGgufInt32 = 5,
    kGgufFloat32 = 6, kGgufBool = 7, kGgufString = 8, kGgufArray = 9, kGgufUint64 = 10, kGgufInt64 = 11,
    kGgufFloat64 = 12,
};

// ggml tensor element types this engine can execute.
enum GgmlType : uint32_t { kGgmlF32 = 0, kGgmlF16 = 1, kGgmlQ4K = 12, kGgmlQ6K = 14, kGgmlBF16 = 30 };

constexpr int kQkK = 256;
constexpr size_t kQ4KBlockBytes = 144;  // f16 d, f16 dmin, scales[12], qs[128]
constexpr size_t kQ6KBlockBytes = 210;  // ql[128], qh[64], scales[16], f16 d

struct GgufKv {
    uint32_t type = 0;
    uint64_t u = 0;           // unsigned/bool scalars
    int64_t i = 0;            // signed scalars
    double f = 0.0;           // float scalars
    std::string_view str;     // string scalar
    uint32_t array_type = 0;  // element type for arrays
    uint64_t array_count = 0;
    const uint8_t* array_data = nullptr;          // numeric arrays (packed, little-endian)
    std::vector<std::string_view> array_strings;  // string arrays
};

struct GgufTensor {
    std::string name;
    uint32_t type = 0;
    uint32_t n_dims = 0;
    int64_t ne[4] = {1, 1, 1, 1};  // ne[0] is the contiguous (row) dimension
    uint64_t offset = 0;           // relative to the data section
    size_t bytes = 0;
    size_t row_bytes = 0;
    const uint8_t* data = nullptr;
};

struct GgufFile {
    void* mapping = nullptr;  // platform handle state, owned
    const uint8_t* base = nullptr;
    size_t size = 0;
    uint32_t version = 0;
    size_t alignment = 32;
    std::map<std::string, GgufKv, std::less<>> kv;
    std::vector<GgufTensor> tensors;
    std::map<std::string, size_t, std::less<>> tensor_index;
};

bool GgufOpen(const std::string& path, GgufFile* file, std::string* error);
void GgufClose(GgufFile* file);

const GgufKv* GgufFind(const GgufFile& file, std::string_view key);
const GgufTensor* GgufFindTensor(const GgufFile& file, std::string_view name);
bool GgufTypeRowBytes(uint32_t type, int64_t elements, size_t* bytes);
const char* GgmlTypeName(uint32_t type);

// Typed accessors; false when absent or of another type.
bool GgufGetU32(const GgufFile& file, std::string_view key, uint32_t* value);
bool GgufGetF32(const GgufFile& file, std::string_view key, float* value);
bool GgufGetBool(const GgufFile& file, std::string_view key, bool* value);
bool GgufGetString(const GgufFile& file, std::string_view key, std::string_view* value);

}  // namespace liminal::engine
