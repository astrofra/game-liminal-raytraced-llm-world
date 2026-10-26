#include "scene_hash.h"
namespace liminal {
namespace {
// Same vendored public-domain implementation as the existing scene archives.
#include "sha256/sha256.c"
}
std::string SceneTextHash(const std::string& text) {
    unsigned char digest[32];sha256_hash(digest,reinterpret_cast<const unsigned char*>(text.data()),text.size());
    const char* hex="0123456789abcdef";std::string out;
    for(unsigned char c:digest){out+=hex[c>>4];out+=hex[c&15];}return out;
}
}
