#ifndef LIMINAL_ARCHIVE_CAMERA_H
#define LIMINAL_ARCHIVE_CAMERA_H

#include "scene.h"
#include <cmath>
#include <stdexcept>

namespace liminal {

// Tight projected bounds of actual vertices, excluding large horizontal floor
// triangles from framing only. Those triangles remain in the rendered scene.
inline void FrameArchiveScene(Scene* scene, RenderConfig* config,
                              float azimuth_degrees, float elevation_degrees)
{
    const float azimuth = azimuth_degrees * kPi / 180.0f;
    const float elevation = elevation_degrees * kPi / 180.0f;
    const Vec3 backward(std::sin(azimuth) * std::cos(elevation),
                        std::sin(elevation), -std::cos(azimuth) * std::cos(elevation));
    const Vec3 forward = -backward;
    const Vec3 right = Normalize(Cross(forward, Vec3(0, 1, 0)));
    const Vec3 up = Normalize(Cross(right, forward));
    Aabb projected;
    bool any = false;
    auto include = [&](const Vec3& p) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            throw std::runtime_error("Non-finite scene vertex");
        projected.Expand(Vec3(Dot(p, right), Dot(p, up), Dot(p, backward)));
        any = true;
    };
    // Boxes retain the exact rotations used by the scene loader. Include glass
    // support triangles too, without changing the loaded scene.
    for (const auto& box : scene->boxes)
        for (const auto& p : box.corners) include(p);
    for (const auto& triangle : scene->triangles) {
        const bool horizontal = std::abs(triangle.v0.y - triangle.v1.y) < .001f &&
                                std::abs(triangle.v0.y - triangle.v2.y) < .001f;
        if (horizontal && !scene->boxes.empty()) continue;
        include(triangle.v0); include(triangle.v1); include(triangle.v2);
    }
    if (!any) throw std::runtime_error("Scene has no geometry to frame");
    const Vec3 center = (projected.min + projected.max) * .5f;
    const Vec3 extent = projected.max - projected.min;
    const float aspect = static_cast<float>(config->width) / config->height;
    config->orthographic_height = std::max(1.0f, std::max(extent.x / aspect, extent.y) * 1.18f);
    scene->camera.target = right * center.x + up * center.y + backward * center.z;
    // Place the ray origins ahead of ALL geometry, including the floor.
    float depth = projected.max.z;
    for (const auto& triangle : scene->triangles)
        for (const auto& p : {triangle.v0, triangle.v1, triangle.v2})
            depth = std::max(depth, Dot(p, backward));
    scene->camera.eye = scene->camera.target + backward * (depth - center.z + config->orthographic_height);
    scene->camera.up = Vec3(0, 1, 0);
    // The player's lamp must not move to the overview camera. Sun, sky and
    // emissive geometry are unchanged; no visor distortion or HUD is applied.
    scene->camera_spotlight.enabled = false;
}

} // namespace liminal
#endif

