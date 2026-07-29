#include "scene.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

namespace liminal {

namespace {

struct MaterialDraft {
    std::string name;
    Vec3 ka;
    Vec3 kd;
    bool has_ka;
    bool has_kd;

    MaterialDraft() : ka(0.0f), kd(0.0f), has_ka(false), has_kd(false) {}
};

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument);
}

static std::string DirectoryOf(const char* path)
{
    if (!path) {
        return std::string();
    }

    const char* last_slash = strrchr(path, '\\');
    const char* last_alt_slash = strrchr(path, '/');
    const char* split = last_slash;
    if (last_alt_slash && (!split || last_alt_slash > split)) {
        split = last_alt_slash;
    }

    if (!split) {
        return std::string();
    }

    return std::string(path, split - path + 1);
}

static std::string JoinPath(const std::string& directory, const char* leaf)
{
    if (!leaf || !leaf[0]) {
        return directory;
    }

    if (directory.empty()) {
        return std::string(leaf);
    }

    return directory + leaf;
}

static bool ParseVec3(const char* text, Vec3* value)
{
    if (!text || !value) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (sscanf(text, "%f %f %f", &x, &y, &z) != 3) {
        return false;
    }

    *value = Vec3(x, y, z);
    return true;
}

static Material FinalizeMaterial(const MaterialDraft& draft)
{
    Material material;
    material.name = draft.name;

    const Vec3 kd = draft.has_kd ? draft.kd : Vec3(0.6f);
    const Vec3 ka = draft.has_ka ? draft.ka : kd;

    const float kd_luminance = Luminance(kd);
    const float ka_luminance = Luminance(ka);
    const bool emissive = (draft.name == "light") || kd_luminance > 1.0f || ka_luminance > 1.0f;

    if (emissive) {
        material.albedo = 0.0f;
        material.emission = std::max(kd_luminance, ka_luminance);
    } else {
        material.albedo = Clamp(kd_luminance > 0.0f ? kd_luminance : ka_luminance, 0.05f, 0.95f);
        material.emission = 0.0f;
    }

    return material;
}

static int EnsureMaterial(
    const std::string& name,
    std::vector<Material>* materials,
    std::unordered_map<std::string, int>* material_indices)
{
    std::unordered_map<std::string, int>::const_iterator found = material_indices->find(name);
    if (found != material_indices->end()) {
        return found->second;
    }

    Material material;
    material.name = name;
    materials->push_back(material);

    const int index = static_cast<int>(materials->size()) - 1;
    (*material_indices)[name] = index;
    return index;
}

static bool LoadMaterialLibrary(
    const char* mtl_path,
    std::vector<Material>* materials,
    std::unordered_map<std::string, int>* material_indices,
    char* error_buffer,
    size_t error_buffer_size)
{
    FILE* file = fopen(mtl_path, "rb");
    if (!file) {
        SetError(error_buffer, error_buffer_size, "Cannot open material file: %s", mtl_path);
        return false;
    }

    MaterialDraft current;
    bool has_current = false;

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "newmtl ", 7) == 0) {
            if (has_current && !current.name.empty()) {
                const int index = EnsureMaterial(current.name, materials, material_indices);
                (*materials)[index] = FinalizeMaterial(current);
            }

            current = MaterialDraft();
            current.name = std::string(line + 7);
            while (!current.name.empty() &&
                   (current.name[current.name.size() - 1] == '\n' ||
                    current.name[current.name.size() - 1] == '\r' ||
                    current.name[current.name.size() - 1] == ' ' ||
                    current.name[current.name.size() - 1] == '\t')) {
                current.name.erase(current.name.size() - 1);
            }
            has_current = true;
            continue;
        }

        if (!has_current) {
            continue;
        }

        if (strncmp(line, "Kd ", 3) == 0) {
            current.has_kd = ParseVec3(line + 3, &current.kd);
        } else if (strncmp(line, "Ka ", 3) == 0) {
            current.has_ka = ParseVec3(line + 3, &current.ka);
        }
    }

    if (has_current && !current.name.empty()) {
        const int index = EnsureMaterial(current.name, materials, material_indices);
        (*materials)[index] = FinalizeMaterial(current);
    }

    fclose(file);
    return true;
}

static Triangle MakeTriangle(const Vec3& a, const Vec3& b, const Vec3& c, int material_index)
{
    Triangle triangle;
    triangle.v0 = a;
    triangle.v1 = b;
    triangle.v2 = c;
    triangle.material_index = material_index;
    triangle.bounds.Expand(a);
    triangle.bounds.Expand(b);
    triangle.bounds.Expand(c);
    triangle.centroid = (a + b + c) / 3.0f;

    const Vec3 edge1 = b - a;
    const Vec3 edge2 = c - a;
    const Vec3 cross = Cross(edge1, edge2);
    triangle.area = 0.5f * Length(cross);
    triangle.normal = triangle.area > kEpsilon ? Normalize(cross) : Vec3(0.0f, 1.0f, 0.0f);

    return triangle;
}

static int LargestAxis(const Vec3& extent)
{
    if (extent.x > extent.y && extent.x > extent.z) {
        return 0;
    }
    if (extent.y > extent.z) {
        return 1;
    }
    return 2;
}

static float AxisValue(const Vec3& value, int axis)
{
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

static Aabb ComputeTriangleRangeBounds(const std::vector<Triangle>& triangles, int start, int end)
{
    Aabb bounds;
    for (int index = start; index < end; ++index) {
        bounds.Expand(triangles[index].bounds);
    }
    return bounds;
}

static Aabb ComputeCentroidRangeBounds(const std::vector<Triangle>& triangles, int start, int end)
{
    Aabb bounds;
    for (int index = start; index < end; ++index) {
        bounds.Expand(triangles[index].centroid);
    }
    return bounds;
}

static int BuildBvhRecursive(Scene* scene, int start, int end)
{
    const int node_index = static_cast<int>(scene->bvh_nodes.size());
    scene->bvh_nodes.push_back(BvhNode());

    BvhNode node;
    node.bounds = ComputeTriangleRangeBounds(scene->triangles, start, end);

    const int triangle_count = end - start;
    if (triangle_count <= 4) {
        node.first_triangle = start;
        node.triangle_count = triangle_count;
        scene->bvh_nodes[node_index] = node;
        return node_index;
    }

    const Aabb centroid_bounds = ComputeCentroidRangeBounds(scene->triangles, start, end);
    const Vec3 extent = centroid_bounds.max - centroid_bounds.min;
    const int axis = LargestAxis(extent);
    const int middle = start + triangle_count / 2;

    std::nth_element(
        scene->triangles.begin() + start,
        scene->triangles.begin() + middle,
        scene->triangles.begin() + end,
        [axis](const Triangle& a, const Triangle& b) {
            return AxisValue(a.centroid, axis) < AxisValue(b.centroid, axis);
        });

    node.left = BuildBvhRecursive(scene, start, middle);
    node.right = BuildBvhRecursive(scene, middle, end);

    scene->bvh_nodes[node_index] = node;
    return node_index;
}

static void AddCornellAreaLight(Scene* scene, const Vec3& center, int material_index)
{
    const float half_width = 65.0f;
    const float half_depth = 52.5f;

    const Vec3 a(center.x - half_width, center.y, center.z - half_depth);
    const Vec3 b(center.x + half_width, center.y, center.z - half_depth);
    const Vec3 c(center.x + half_width, center.y, center.z + half_depth);
    const Vec3 d(center.x - half_width, center.y, center.z + half_depth);

    scene->triangles.push_back(MakeTriangle(a, c, d, material_index));
    scene->triangles.push_back(MakeTriangle(a, b, c, material_index));
}

static bool ParseFaceToken(const char* token, int vertex_count, int* index)
{
    if (!token || !token[0] || !index) {
        return false;
    }

    char* end = 0;
    long value = strtol(token, &end, 10);
    if (end == token) {
        return false;
    }

    if (value > 0) {
        *index = static_cast<int>(value - 1);
        return *index >= 0 && *index < vertex_count;
    }

    if (value < 0) {
        *index = vertex_count + static_cast<int>(value);
        return *index >= 0 && *index < vertex_count;
    }

    return false;
}

static void CollectEmissiveTriangles(Scene* scene)
{
    scene->emissive_triangles.clear();
    for (size_t index = 0; index < scene->triangles.size(); ++index) {
        const Triangle& triangle = scene->triangles[index];
        const Material& material = scene->materials[triangle.material_index];
        if (material.emission > 0.0f) {
            scene->emissive_triangles.push_back(static_cast<int>(index));
        }
    }
}

}  // namespace

bool LoadSceneFromObj(const char* obj_path, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    if (!obj_path || !scene) {
        SetError(error_buffer, error_buffer_size, "Invalid OBJ path: %s", "(null)");
        return false;
    }

    scene->materials.clear();
    scene->triangles.clear();
    scene->emissive_triangles.clear();
    scene->bvh_nodes.clear();
    scene->bounds = Aabb();
    scene->camera = Camera();

    std::vector<Vec3> positions;
    std::unordered_map<std::string, int> material_indices;

    Material default_material;
    default_material.name = "default";
    default_material.albedo = 0.6f;
    default_material.emission = 0.0f;
    scene->materials.push_back(default_material);
    material_indices[default_material.name] = 0;

    FILE* file = fopen(obj_path, "rb");
    if (!file) {
        SetError(error_buffer, error_buffer_size, "Cannot open OBJ file: %s", obj_path);
        return false;
    }

    const std::string base_directory = DirectoryOf(obj_path);

    int current_material = 0;
    bool in_camera_block = false;
    bool in_light_block = false;
    int camera_vertex_count = 0;
    Vec3 light_center(278.0f, 548.5f, 279.5f);
    bool has_light_center = false;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "#camera", 7) == 0) {
            in_camera_block = true;
            in_light_block = false;
            camera_vertex_count = 0;
            continue;
        }

        if (strncmp(line, "# light", 7) == 0) {
            in_light_block = true;
            in_camera_block = false;
            continue;
        }

        if (in_camera_block) {
            if (strncmp(line, "v ", 2) == 0) {
                Vec3 value;
                if (ParseVec3(line + 2, &value)) {
                    if (camera_vertex_count == 0) {
                        scene->camera.eye = value;
                    } else if (camera_vertex_count == 1) {
                        scene->camera.target = value;
                    }
                    ++camera_vertex_count;
                }
                continue;
            }

            if (strncmp(line, "vn ", 3) == 0) {
                Vec3 value;
                if (ParseVec3(line + 3, &value)) {
                    scene->camera.up = value;
                }
                continue;
            }

            if (line[0] == 'c') {
                in_camera_block = false;
                continue;
            }
        }

        if (in_light_block && strncmp(line, "v ", 2) == 0) {
            if (ParseVec3(line + 2, &light_center)) {
                has_light_center = true;
            }
            continue;
        }

        if (strncmp(line, "mtllib ", 7) == 0) {
            char relative_path[512];
            if (sscanf(line + 7, "%511s", relative_path) == 1) {
                const std::string full_path = JoinPath(base_directory, relative_path);
                if (!LoadMaterialLibrary(
                        full_path.c_str(),
                        &scene->materials,
                        &material_indices,
                        error_buffer,
                        error_buffer_size)) {
                    fclose(file);
                    return false;
                }
            }
            continue;
        }

        if (strncmp(line, "usemtl ", 7) == 0) {
            char material_name[256];
            if (sscanf(line + 7, "%255s", material_name) == 1) {
                current_material = EnsureMaterial(material_name, &scene->materials, &material_indices);
            }
            in_light_block = false;
            continue;
        }

        if (strncmp(line, "v ", 2) == 0) {
            Vec3 value;
            if (ParseVec3(line + 2, &value)) {
                positions.push_back(value);
            }
            continue;
        }

        if (strncmp(line, "f ", 2) == 0) {
            char buffer[1024];
            memcpy(buffer, line, sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            std::vector<int> indices;
            char* context = 0;
            char* token = strtok_s(buffer, " \t\r\n", &context);
            token = strtok_s(0, " \t\r\n", &context);

            while (token) {
                int index = -1;
                if (ParseFaceToken(token, static_cast<int>(positions.size()), &index)) {
                    indices.push_back(index);
                }
                token = strtok_s(0, " \t\r\n", &context);
            }

            if (indices.size() < 3) {
                continue;
            }

            for (size_t face_index = 1; face_index + 1 < indices.size(); ++face_index) {
                const Triangle triangle = MakeTriangle(
                    positions[indices[0]],
                    positions[indices[face_index]],
                    positions[indices[face_index + 1]],
                    current_material);
                scene->triangles.push_back(triangle);
                scene->bounds.Expand(triangle.bounds);
            }
        }
    }

    fclose(file);

    int light_material_index = EnsureMaterial("light", &scene->materials, &material_indices);
    Material& light_material = scene->materials[light_material_index];
    if (light_material.emission <= 0.0f) {
        light_material.albedo = 0.0f;
        light_material.emission = 12.0f;
    }

    if (has_light_center) {
        AddCornellAreaLight(scene, light_center, light_material_index);
    }

    scene->bounds = Aabb();
    for (size_t index = 0; index < scene->triangles.size(); ++index) {
        scene->bounds.Expand(scene->triangles[index].bounds);
    }

    scene->bvh_nodes.clear();
    if (!scene->triangles.empty()) {
        BuildBvhRecursive(scene, 0, static_cast<int>(scene->triangles.size()));
    }

    CollectEmissiveTriangles(scene);
    return true;
}

}  // namespace liminal
