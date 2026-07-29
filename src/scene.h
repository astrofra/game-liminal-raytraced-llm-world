#ifndef LIMINAL_RENDERER_SCENE_H
#define LIMINAL_RENDERER_SCENE_H

#include <stddef.h>
#include <vector>

#include "core.h"

namespace liminal {

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
    std::vector<Material> materials;
    std::vector<Triangle> triangles;
    std::vector<int> emissive_triangles;
    std::vector<BvhNode> bvh_nodes;
    Aabb bounds;
    Camera camera;
};

bool LoadSceneFromObj(const char* obj_path, Scene* scene, char* error_buffer, size_t error_buffer_size);

}  // namespace liminal

#endif
