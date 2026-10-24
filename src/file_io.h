#ifndef LIMINAL_FILE_IO_H
#define LIMINAL_FILE_IO_H

#include <stdio.h>
#include <errno.h>
#include <string>
#if defined(_WIN32)
#include <filesystem>
#endif

namespace liminal {

// Public paths are UTF-8, including paths received from Windows wide argv.
inline FILE* OpenUtf8File(const char* path, const char* mode)
{
    if (!path || !mode) { errno = EINVAL; return nullptr; }
#if defined(_WIN32)
    try {
        const std::string narrow_mode(mode);
        const std::wstring wide_mode(narrow_mode.begin(), narrow_mode.end());
        return _wfopen(std::filesystem::u8path(path).c_str(), wide_mode.c_str());
    } catch (...) {
        errno = EINVAL;
        return nullptr;
    }
#else
    return fopen(path, mode);
#endif
}

} // namespace liminal
#endif
