#ifndef LIMINAL_RENDERER_SCENE_H
#define LIMINAL_RENDERER_SCENE_H

#include <stddef.h>
#include <array>
#include <vector>

#include "core.h"

namespace liminal {

// Original authoring boxes, retained before BVH triangle reordering. The same
// transform drives rendering and support/contact geometry (Euler X, Y, Z).
struct SceneBox {
    std::string id;
    Vec3 center, size;
    std::array<Vec3, 3> axes;
    std::array<Vec3, 8> corners;
    bool glass = false;
};
SceneBox MakeSceneBox(const std::string& id, const Vec3& center, const Vec3& size,
                     const Vec3& rotation, bool glass = false);
struct SceneBoxFace {
    std::array<Vec3,4> corners;
    Vec3 normal, center;
};
// Stable face order: +Z, -Z, -X, +X, +Y, -Y. Also used to triangulate boxes.
std::array<SceneBoxFace,6> SceneBoxFaces(const SceneBox& box);
bool SceneBoxesTouch(const SceneBox& a,const SceneBox& b,float tolerance=.01f);

struct BvhNode {
    Aabb bounds;
    int left;
    int right;
    int first_triangle;
    int triangle_count;

    BvhNode() : left(-1), right(-1), first_triangle(0), triangle_count(0) {}

    bool IsLeaf() const
    {
        return left < 0 && right < 0;
    }
};

struct Scene {
    std::string name;
    CameraSpotlight camera_spotlight;
    DistantLight distant_light;
    SkyBackground sky_background;
    std::vector<Material> materials;
    std::vector<SceneBox> boxes;
    std::vector<Triangle> triangles;
    std::vector<int> emissive_triangles;
    std::vector<BvhNode> bvh_nodes;
    Aabb bounds;
    Camera camera;
};

bool LoadSceneFromPath(const char* scene_path, Scene* scene, char* error_buffer, size_t error_buffer_size);
bool LoadSceneFromObj(const char* obj_path, Scene* scene, char* error_buffer, size_t error_buffer_size);
bool LoadSceneFromSceneV1(const char* scene_path, Scene* scene, char* error_buffer, size_t error_buffer_size);
bool LoadSceneFromSceneText(
    const char* scene_name,
    const char* scene_text,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size);

}  // namespace liminal

#endif
