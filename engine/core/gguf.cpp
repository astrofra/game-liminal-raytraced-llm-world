#include "core/gguf.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace liminal::engine {

namespace {

struct Reader {
    const uint8_t* base;
    size_t size;
    size_t at = 0;
    bool failed = false;

    bool Take(size_t n, const uint8_t** out) {
        if (failed || n > size - at) { failed = true; return false; }
        *out = base + at;
        at += n;
        return true;
    }
    template <typename T> bool Read(T* value) {
        const uint8_t* p = nullptr;
        if (!Take(sizeof(T), &p)) return false;
        memcpy(value, p, sizeof(T));
        return true;
    }
    bool ReadString(std::string_view* value) {
        uint64_t n = 0;
        const uint8_t* p = nullptr;
        if (!Read(&n) || n > size || !Take(static_cast<size_t>(n), &p)) { failed = true; return false; }
        *value = std::string_view(reinterpret_cast<const char*>(p), static_cast<size_t>(n));
        return true;
    }
};

static size_t ScalarBytes(uint32_t type)
{
    switch (type) {
        case kGgufUint8: case kGgufInt8: case kGgufBool: return 1;
        case kGgufUint16: case kGgufInt16: return 2;
        case kGgufUint32: case kGgufInt32: case kGgufFloat32: return 4;
        case kGgufUint64: case kGgufInt64: case kGgufFloat64: return 8;
        default: return 0;
    }
}

static bool ReadScalar(Reader* reader, uint32_t type, GgufKv* kv)
{
    switch (type) {
        case kGgufUint8: { uint8_t v; if (!reader->Read(&v)) return false; kv->u = v; kv->i = v; kv->f = v; return true; }
        case kGgufBool: { uint8_t v; if (!reader->Read(&v)) return false; kv->u = v != 0; return true; }
        case kGgufInt8: { int8_t v; if (!reader->Read(&v)) return false; kv->i = v; kv->u = static_cast<uint64_t>(v); kv->f = v; return true; }
        case kGgufUint16: { uint16_t v; if (!reader->Read(&v)) return false; kv->u = v; kv->i = v; kv->f = v; return true; }
        case kGgufInt16: { int16_t v; if (!reader->Read(&v)) return false; kv->i = v; kv->u = static_cast<uint64_t>(v); kv->f = v; return true; }
        case kGgufUint32: { uint32_t v; if (!reader->Read(&v)) return false; kv->u = v; kv->i = v; kv->f = v; return true; }
        case kGgufInt32: { int32_t v; if (!reader->Read(&v)) return false; kv->i = v; kv->u = static_cast<uint64_t>(v); kv->f = v; return true; }
        case kGgufFloat32: { float v; if (!reader->Read(&v)) return false; kv->f = v; return true; }
        case kGgufUint64: { uint64_t v; if (!reader->Read(&v)) return false; kv->u = v; kv->i = static_cast<int64_t>(v); kv->f = static_cast<double>(v); return true; }
        case kGgufInt64: { int64_t v; if (!reader->Read(&v)) return false; kv->i = v; kv->u = static_cast<uint64_t>(v); kv->f = static_cast<double>(v); return true; }
        case kGgufFloat64: { double v; if (!reader->Read(&v)) return false; kv->f = v; return true; }
        case kGgufString: return reader->ReadString(&kv->str);
        default: return false;
    }
}

static bool MapFile(const std::string& path, GgufFile* file, std::string* error)
{
#if defined(_WIN32)
    const std::wstring wide = std::filesystem::u8path(path).wstring();
    HANDLE handle = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { *error = "Cannot open " + path; return false; }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart <= 0) { CloseHandle(handle); *error = "Empty model file"; return false; }
    HANDLE mapping = CreateFileMappingW(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(handle);
    if (!mapping) { *error = "Cannot map " + path; return false; }
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) { CloseHandle(mapping); *error = "Cannot map " + path; return false; }
    file->mapping = mapping;
    file->base = static_cast<const uint8_t*>(view);
    file->size = static_cast<size_t>(size.QuadPart);
#else
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) { *error = "Cannot open " + path; return false; }
    struct stat info {};
    if (fstat(fd, &info) != 0 || info.st_size <= 0) { close(fd); *error = "Empty model file"; return false; }
    void* view = mmap(nullptr, static_cast<size_t>(info.st_size), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (view == MAP_FAILED) { *error = "Cannot map " + path; return false; }
    file->mapping = view;
    file->base = static_cast<const uint8_t*>(view);
    file->size = static_cast<size_t>(info.st_size);
#endif
    return true;
}

}  // namespace

bool GgufTypeRowBytes(uint32_t type, int64_t elements, size_t* bytes)
{
    if (elements < 0) return false;
    const uint64_t n = static_cast<uint64_t>(elements);
    switch (type) {
        case kGgmlF32: *bytes = static_cast<size_t>(n * 4); return true;
        case kGgmlF16: case kGgmlBF16: *bytes = static_cast<size_t>(n * 2); return true;
        case kGgmlQ4K: if (n % kQkK) return false; *bytes = static_cast<size_t>(n / kQkK * kQ4KBlockBytes); return true;
        case kGgmlQ6K: if (n % kQkK) return false; *bytes = static_cast<size_t>(n / kQkK * kQ6KBlockBytes); return true;
        default: return false;
    }
}

const char* GgmlTypeName(uint32_t type)
{
    switch (type) {
        case kGgmlF32: return "F32";
        case kGgmlF16: return "F16";
        case kGgmlBF16: return "BF16";
        case kGgmlQ4K: return "Q4_K";
        case kGgmlQ6K: return "Q6_K";
        default: return "unsupported";
    }
}

bool GgufOpen(const std::string& path, GgufFile* file, std::string* error)
{
    GgufClose(file);
    if (!MapFile(path, file, error)) return false;
    Reader reader{file->base, file->size};
    const auto fail = [&](const std::string& message) { *error = message; GgufClose(file); return false; };
    const uint8_t* magic = nullptr;
    if (!reader.Take(4, &magic) || memcmp(magic, "GGUF", 4) != 0) return fail("Not a GGUF file");
    uint64_t tensor_count = 0, kv_count = 0;
    if (!reader.Read(&file->version) || !reader.Read(&tensor_count) || !reader.Read(&kv_count))
        return fail("Truncated GGUF header");
    if (file->version != 3) return fail("Unsupported GGUF version " + std::to_string(file->version));
    if (tensor_count > 100000 || kv_count > 100000) return fail("Implausible GGUF header counts");

    for (uint64_t k = 0; k < kv_count; ++k) {
        std::string_view key;
        uint32_t type = 0;
        if (!reader.ReadString(&key) || !reader.Read(&type)) return fail("Truncated GGUF metadata");
        GgufKv kv;
        kv.type = type;
        if (type == kGgufArray) {
            if (!reader.Read(&kv.array_type) || !reader.Read(&kv.array_count)) return fail("Truncated GGUF array");
            if (kv.array_type == kGgufString) {
                if (kv.array_count > file->size) return fail("Implausible GGUF string array");
                kv.array_strings.resize(static_cast<size_t>(kv.array_count));
                for (auto& item : kv.array_strings) if (!reader.ReadString(&item)) return fail("Truncated GGUF string array");
            } else {
                const size_t element = ScalarBytes(kv.array_type);
                if (!element || kv.array_count > file->size / element) return fail("Unsupported GGUF array in " + std::string(key));
                if (!reader.Take(static_cast<size_t>(kv.array_count) * element, &kv.array_data)) return fail("Truncated GGUF array data");
            }
        } else if (!ReadScalar(&reader, type, &kv)) {
            return fail("Invalid GGUF metadata value for " + std::string(key));
        }
        file->kv.emplace(std::string(key), std::move(kv));
    }
    if (const GgufKv* alignment = GgufFind(*file, "general.alignment")) {
        if (alignment->type != kGgufUint32 || alignment->u == 0 || (alignment->u & (alignment->u - 1)))
            return fail("Invalid general.alignment");
        file->alignment = static_cast<size_t>(alignment->u);
    }

    file->tensors.resize(static_cast<size_t>(tensor_count));
    for (auto& tensor : file->tensors) {
        std::string_view name;
        if (!reader.ReadString(&name) || !reader.Read(&tensor.n_dims) || tensor.n_dims < 1 || tensor.n_dims > 4)
            return fail("Invalid GGUF tensor header");
        tensor.name = std::string(name);
        for (uint32_t d = 0; d < tensor.n_dims; ++d) {
            uint64_t ne = 0;
            if (!reader.Read(&ne) || ne == 0 || ne > (uint64_t(1) << 40)) return fail("Invalid dimension for " + tensor.name);
            tensor.ne[d] = static_cast<int64_t>(ne);
        }
        if (!reader.Read(&tensor.type) || !reader.Read(&tensor.offset)) return fail("Truncated tensor header for " + tensor.name);
        if (!GgufTypeRowBytes(tensor.type, tensor.ne[0], &tensor.row_bytes))
            return fail("Unsupported tensor type " + std::to_string(tensor.type) + " for " + tensor.name);
        const uint64_t rows = static_cast<uint64_t>(tensor.ne[1]) * static_cast<uint64_t>(tensor.ne[2]) * static_cast<uint64_t>(tensor.ne[3]);
        if (rows > file->size / std::max<size_t>(tensor.row_bytes, 1)) return fail("Tensor too large: " + tensor.name);
        tensor.bytes = static_cast<size_t>(rows) * tensor.row_bytes;
        if (!file->tensor_index.emplace(tensor.name, static_cast<size_t>(&tensor - file->tensors.data())).second)
            return fail("Duplicate tensor " + tensor.name);
    }
    const size_t data_start = (reader.at + file->alignment - 1) / file->alignment * file->alignment;
    if (data_start > file->size) return fail("Truncated GGUF tensor data");
    for (auto& tensor : file->tensors) {
        if (tensor.offset % file->alignment || tensor.offset > file->size - data_start ||
            tensor.bytes > file->size - data_start - tensor.offset)
            return fail("Tensor data out of bounds: " + tensor.name);
        tensor.data = file->base + data_start + tensor.offset;
    }
    return true;
}

void GgufClose(GgufFile* file)
{
    if (file->mapping) {
#if defined(_WIN32)
        UnmapViewOfFile(file->base);
        CloseHandle(static_cast<HANDLE>(file->mapping));
#else
        munmap(file->mapping, file->size);
#endif
    }
    *file = GgufFile();
}

const GgufKv* GgufFind(const GgufFile& file, std::string_view key)
{
    const auto it = file.kv.find(key);
    return it == file.kv.end() ? nullptr : &it->second;
}

const GgufTensor* GgufFindTensor(const GgufFile& file, std::string_view name)
{
    const auto it = file.tensor_index.find(name);
    return it == file.tensor_index.end() ? nullptr : &file.tensors[it->second];
}

bool GgufGetU32(const GgufFile& file, std::string_view key, uint32_t* value)
{
    const GgufKv* kv = GgufFind(file, key);
    if (!kv || (kv->type != kGgufUint32 && kv->type != kGgufInt32) || kv->i < 0) return false;
    *value = static_cast<uint32_t>(kv->u);
    return true;
}

bool GgufGetF32(const GgufFile& file, std::string_view key, float* value)
{
    const GgufKv* kv = GgufFind(file, key);
    if (!kv || kv->type != kGgufFloat32) return false;
    *value = static_cast<float>(kv->f);
    return true;
}

bool GgufGetBool(const GgufFile& file, std::string_view key, bool* value)
{
    const GgufKv* kv = GgufFind(file, key);
    if (!kv || kv->type != kGgufBool) return false;
    *value = kv->u != 0;
    return true;
}

bool GgufGetString(const GgufFile& file, std::string_view key, std::string_view* value)
{
    const GgufKv* kv = GgufFind(file, key);
    if (!kv || kv->type != kGgufString) return false;
    *value = kv->str;
    return true;
}

}  // namespace liminal::engine
