#ifndef LIMINAL_GLASS_SUPPORTS_H
#define LIMINAL_GLASS_SUPPORTS_H

#include "scene.h"

namespace liminal {
struct GlassSupportExclusion { std::string name; Vec3 center, size; };
struct GlassSupportOptions {
    bool build_supports = true;
    bool protect_local_approaches = true;
    size_t max_triangles = 4096;
    std::vector<GlassSupportExclusion> reserved;
};
struct GlassSupportResult {
    bool success = false;
    std::string error, opaque_scene_text, scene_text, report_json;
    size_t added_supports = 0, added_triangles = 0;
};
// Engine-only augmentation. Failure preserves the input; never repairs opaque
// geometry. Reports measure connectivity, not mechanical stability.
GlassSupportResult BuildGlassSupports(const std::string& program,
                                     const GlassSupportOptions& options = {});
// Strip only our complete trailing envelope; used for archives and LLM context.
std::string OpaqueSceneWithoutGlassSupports(const std::string& program);
} // namespace liminal
#endif
