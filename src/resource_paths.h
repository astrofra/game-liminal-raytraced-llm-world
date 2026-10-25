#pragma once
#include <filesystem>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace liminal {
// Resolve packaged assets beside the executable; bounded ancestor search also
// supports development build/Release without depending on the current directory.
inline std::filesystem::path ResourceRoot() {
    static const auto root = [] {
        std::filesystem::path executable;
#if defined(_WIN32)
        std::wstring buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length > 0 && length < buffer.size()) executable = buffer.substr(0, length);
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) == 0) executable = buffer.c_str();
#else
        char buffer[4096];
        const auto length = readlink("/proc/self/exe", buffer, sizeof(buffer));
        if (length > 0) executable = std::string(buffer, static_cast<size_t>(length));
#endif
        const auto base = executable.parent_path();
        auto candidate = base;
        for (int depth=0; depth<4 && !candidate.empty(); ++depth) {
            if (std::filesystem::is_regular_file(candidate / "assets/scenes/eryx_quarry_threshold.scene")) return candidate;
            candidate = candidate.parent_path();
        }
        return base;
    }();
    return root;
}
inline std::string ResourcePath(const std::string& path) {
    return (ResourceRoot() / std::filesystem::u8path(path)).u8string();
}
}
