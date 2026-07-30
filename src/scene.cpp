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

struct FaceQuad {
    int i0;
    int i1;
    int i2;
    int i3;
    Vec3 expected_normal;
};

static void ResetScene(Scene* scene)
{
    scene->name.clear();
    scene->camera_spotlight = CameraSpotlight();
    scene->materials.clear();
    scene->triangles.clear();
    scene->emissive_triangles.clear();
    scene->bvh_nodes.clear();
    scene->bounds = Aabb();
    scene->camera = Camera();
}

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument);
}

static void SetLineError(
    char* buffer,
    size_t buffer_size,
    const char* path,
    int line_number,
    const char* message)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, "%s:%d: %s", path ? path : "(scene)", line_number, message);
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

static std::string BasenameOf(const char* path)
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

    return split ? std::string(split + 1) : std::string(path);
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

static std::string Trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() &&
           (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }

    size_t end = value.size();
    while (end > start &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(start, end - start);
}

static std::string StripComment(const std::string& line)
{
    const size_t comment_pos = line.find('#');
    return comment_pos == std::string::npos ? line : line.substr(0, comment_pos);
}

static bool StartsWith(const std::string& line, const char* prefix)
{
    const size_t prefix_size = strlen(prefix);
    return line.size() >= prefix_size && line.compare(0, prefix_size, prefix) == 0;
}

static std::string ToLowerCopy(const std::string& text)
{
    std::string lower = text;
    for (size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] >= 'A' && lower[index] <= 'Z') {
            lower[index] = static_cast<char>(lower[index] - 'A' + 'a');
        }
    }
    return lower;
}

static bool ParseVec3Text(const char* text, Vec3* value)
{
    if (!text || !value) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (sscanf(text, " %f , %f , %f ", &x, &y, &z) != 3) {
        if (sscanf(text, " %f %f %f ", &x, &y, &z) != 3) {
            return false;
        }
    }

    *value = Vec3(x, y, z);
    return true;
}

static bool ParseVec2Text(const char* text, float* x, float* y)
{
    if (!text || !x || !y) {
        return false;
    }

    float vx = 0.0f;
    float vy = 0.0f;
    if (sscanf(text, " %f , %f ", &vx, &vy) != 2) {
        return false;
    }

    *x = vx;
    *y = vy;
    return true;
}

static bool ParseFloatText(const char* text, float* value)
{
    if (!text || !value) {
        return false;
    }

    char* end = 0;
    const float parsed = strtof(text, &end);
    if (end == text) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool ExtractQuotedString(const std::string& line, std::string* value)
{
    if (!value) {
        return false;
    }

    const size_t first_quote = line.find('"');
    if (first_quote == std::string::npos) {
        return false;
    }

    const size_t second_quote = line.find('"', first_quote + 1);
    if (second_quote == std::string::npos || second_quote <= first_quote + 1) {
        return false;
    }

    *value = line.substr(first_quote + 1, second_quote - first_quote - 1);
    return true;
}

static bool ExtractPropertyText(const std::string& line, const char* key, std::string* value)
{
    if (!value) {
        return false;
    }

    const size_t start = line.find(key);
    if (start == std::string::npos) {
        return false;
    }

    const size_t value_start = start + strlen(key);
    const size_t value_end = line.find(')', value_start);
    if (value_end == std::string::npos || value_end <= value_start) {
        return false;
    }

    *value = line.substr(value_start, value_end - value_start);
    return true;
}

static bool ExtractVec3Property(const std::string& line, const char* key, Vec3* value)
{
    std::string text;
    return ExtractPropertyText(line, key, &text) && ParseVec3Text(text.c_str(), value);
}

static bool ExtractVec2Property(const std::string& line, const char* key, float* x, float* y)
{
    std::string text;
    return ExtractPropertyText(line, key, &text) && ParseVec2Text(text.c_str(), x, y);
}

static bool ExtractFloatProperty(const std::string& line, const char* key, float* value)
{
    std::string text;
    return ExtractPropertyText(line, key, &text) && ParseFloatText(text.c_str(), value);
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
    const std::unordered_map<std::string, int>::const_iterator found = material_indices->find(name);
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

static int AddPrimitiveMaterial(Scene* scene, const std::string& name, float gray_value, float emission_value)
{
    Material material;
    material.name = name;
    material.albedo = Clamp(gray_value, 0.0f, 0.95f);
    material.emission = std::max(0.0f, emission_value);
    scene->materials.push_back(material);
    return static_cast<int>(scene->materials.size()) - 1;
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
            current.has_kd = ParseVec3Text(line + 3, &current.kd);
        } else if (strncmp(line, "Ka ", 3) == 0) {
            current.has_ka = ParseVec3Text(line + 3, &current.ka);
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

static Triangle MakeTriangleFacing(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    int material_index,
    const Vec3& expected_normal)
{
    Triangle triangle = MakeTriangle(a, b, c, material_index);
    if (Dot(triangle.normal, expected_normal) < 0.0f) {
        triangle = MakeTriangle(a, c, b, material_index);
    }
    return triangle;
}

static void AddTriangle(Scene* scene, const Triangle& triangle)
{
    scene->triangles.push_back(triangle);
}

static void AddQuad(
    Scene* scene,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d,
    int material_index,
    const Vec3& expected_normal)
{
    AddTriangle(scene, MakeTriangleFacing(a, b, c, material_index, expected_normal));
    AddTriangle(scene, MakeTriangleFacing(a, c, d, material_index, expected_normal));
}

static Vec3 RotateEulerDegrees(const Vec3& point, const Vec3& rotation_degrees)
{
    const float to_radians = kPi / 180.0f;
    const float cx = cosf(rotation_degrees.x * to_radians);
    const float sx = sinf(rotation_degrees.x * to_radians);
    const float cy = cosf(rotation_degrees.y * to_radians);
    const float sy = sinf(rotation_degrees.y * to_radians);
    const float cz = cosf(rotation_degrees.z * to_radians);
    const float sz = sinf(rotation_degrees.z * to_radians);

    Vec3 rotated = point;

    rotated = Vec3(
        rotated.x,
        rotated.y * cx - rotated.z * sx,
        rotated.y * sx + rotated.z * cx);

    rotated = Vec3(
        rotated.x * cy + rotated.z * sy,
        rotated.y,
        -rotated.x * sy + rotated.z * cy);

    rotated = Vec3(
        rotated.x * cz - rotated.y * sz,
        rotated.x * sz + rotated.y * cz,
        rotated.z);

    return rotated;
}

static void AddPlanePrimitive(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& normal,
    float size_x,
    float size_y,
    float gray_value,
    float emission_value)
{
    const int material_index = AddPrimitiveMaterial(scene, name, gray_value, emission_value);
    const Vec3 plane_normal = Normalize(normal);
    const Vec3 tangent_seed = fabsf(plane_normal.y) < 0.999f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 tangent = Normalize(Cross(tangent_seed, plane_normal));
    const Vec3 bitangent = Normalize(Cross(plane_normal, tangent));
    const float half_x = size_x * 0.5f;
    const float half_y = size_y * 0.5f;

    const Vec3 a = center - tangent * half_x - bitangent * half_y;
    const Vec3 b = center + tangent * half_x - bitangent * half_y;
    const Vec3 c = center + tangent * half_x + bitangent * half_y;
    const Vec3 d = center - tangent * half_x + bitangent * half_y;

    AddQuad(scene, a, b, c, d, material_index, plane_normal);
}

static void AddBoxPrimitive(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& size,
    const Vec3& rotation_degrees,
    float gray_value,
    float emission_value)
{
    const int material_index = AddPrimitiveMaterial(scene, name, gray_value, emission_value);
    const Vec3 half_size = size * 0.5f;

    Vec3 corners[8];
    corners[0] = Vec3(-half_size.x, -half_size.y, -half_size.z);
    corners[1] = Vec3(half_size.x, -half_size.y, -half_size.z);
    corners[2] = Vec3(half_size.x, half_size.y, -half_size.z);
    corners[3] = Vec3(-half_size.x, half_size.y, -half_size.z);
    corners[4] = Vec3(-half_size.x, -half_size.y, half_size.z);
    corners[5] = Vec3(half_size.x, -half_size.y, half_size.z);
    corners[6] = Vec3(half_size.x, half_size.y, half_size.z);
    corners[7] = Vec3(-half_size.x, half_size.y, half_size.z);

    for (int index = 0; index < 8; ++index) {
        corners[index] = center + RotateEulerDegrees(corners[index], rotation_degrees);
    }

    FaceQuad faces[6];
    faces[0].i0 = 4; faces[0].i1 = 5; faces[0].i2 = 6; faces[0].i3 = 7;
    faces[0].expected_normal = Normalize(RotateEulerDegrees(Vec3(0.0f, 0.0f, 1.0f), rotation_degrees));
    faces[1].i0 = 1; faces[1].i1 = 0; faces[1].i2 = 3; faces[1].i3 = 2;
    faces[1].expected_normal = Normalize(RotateEulerDegrees(Vec3(0.0f, 0.0f, -1.0f), rotation_degrees));
    faces[2].i0 = 0; faces[2].i1 = 4; faces[2].i2 = 7; faces[2].i3 = 3;
    faces[2].expected_normal = Normalize(RotateEulerDegrees(Vec3(-1.0f, 0.0f, 0.0f), rotation_degrees));
    faces[3].i0 = 5; faces[3].i1 = 1; faces[3].i2 = 2; faces[3].i3 = 6;
    faces[3].expected_normal = Normalize(RotateEulerDegrees(Vec3(1.0f, 0.0f, 0.0f), rotation_degrees));
    faces[4].i0 = 3; faces[4].i1 = 7; faces[4].i2 = 6; faces[4].i3 = 2;
    faces[4].expected_normal = Normalize(RotateEulerDegrees(Vec3(0.0f, 1.0f, 0.0f), rotation_degrees));
    faces[5].i0 = 0; faces[5].i1 = 1; faces[5].i2 = 5; faces[5].i3 = 4;
    faces[5].expected_normal = Normalize(RotateEulerDegrees(Vec3(0.0f, -1.0f, 0.0f), rotation_degrees));

    for (int face_index = 0; face_index < 6; ++face_index) {
        const FaceQuad& face = faces[face_index];
        AddQuad(
            scene,
            corners[face.i0],
            corners[face.i1],
            corners[face.i2],
            corners[face.i3],
            material_index,
            face.expected_normal);
    }
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

    AddQuad(scene, a, b, c, d, material_index, Vec3(0.0f, -1.0f, 0.0f));
}

static bool ParseFaceToken(const char* token, int vertex_count, int* index)
{
    if (!token || !token[0] || !index) {
        return false;
    }

    char* end = 0;
    const long value = strtol(token, &end, 10);
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

static void FinalizeSceneGeometry(Scene* scene)
{
    scene->bounds = Aabb();
    for (size_t index = 0; index < scene->triangles.size(); ++index) {
        scene->bounds.Expand(scene->triangles[index].bounds);
    }

    scene->bvh_nodes.clear();
    if (!scene->triangles.empty()) {
        BuildBvhRecursive(scene, 0, static_cast<int>(scene->triangles.size()));
    }

    CollectEmissiveTriangles(scene);
}

static std::string ExtensionOf(const char* path)
{
    const std::string base_name = BasenameOf(path);
    const size_t dot = base_name.find_last_of('.');
    return dot == std::string::npos ? std::string() : ToLowerCopy(base_name.substr(dot));
}

}  // namespace

bool LoadSceneFromPath(const char* scene_path, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    const std::string extension = ExtensionOf(scene_path);
    if (extension == ".obj") {
        return LoadSceneFromObj(scene_path, scene, error_buffer, error_buffer_size);
    }
    if (extension == ".scene") {
        return LoadSceneFromSceneV1(scene_path, scene, error_buffer, error_buffer_size);
    }

    SetError(error_buffer, error_buffer_size, "Unsupported scene extension: %s", scene_path ? scene_path : "(null)");
    return false;
}

bool LoadSceneFromObj(const char* obj_path, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    if (!obj_path || !scene) {
        SetError(error_buffer, error_buffer_size, "Invalid OBJ path: %s", "(null)");
        return false;
    }

    ResetScene(scene);
    scene->name = BasenameOf(obj_path);

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
                if (ParseVec3Text(line + 2, &value)) {
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
                if (ParseVec3Text(line + 3, &value)) {
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
            if (ParseVec3Text(line + 2, &light_center)) {
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
            if (ParseVec3Text(line + 2, &value)) {
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
                AddTriangle(
                    scene,
                    MakeTriangle(
                        positions[indices[0]],
                        positions[indices[face_index]],
                        positions[indices[face_index + 1]],
                        current_material));
            }
        }
    }

    fclose(file);

    const int light_material_index = EnsureMaterial("light", &scene->materials, &material_indices);
    Material& light_material = scene->materials[light_material_index];
    if (light_material.emission <= 0.0f) {
        light_material.albedo = 0.0f;
        light_material.emission = 12.0f;
    }

    if (has_light_center) {
        AddCornellAreaLight(scene, light_center, light_material_index);
    }

    FinalizeSceneGeometry(scene);
    return true;
}

bool LoadSceneFromSceneV1(const char* scene_path, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    if (!scene_path || !scene) {
        SetError(error_buffer, error_buffer_size, "Invalid scene path: %s", "(null)");
        return false;
    }

    ResetScene(scene);
    scene->name = BasenameOf(scene_path);

    FILE* file = fopen(scene_path, "rb");
    if (!file) {
        SetError(error_buffer, error_buffer_size, "Cannot open scene file: %s", scene_path);
        return false;
    }

    char raw_line[1024];
    int line_number = 0;

    while (fgets(raw_line, sizeof(raw_line), file)) {
        ++line_number;

        std::string line = Trim(StripComment(raw_line));
        if (line.empty()) {
            continue;
        }

        if (StartsWith(line, "room ")) {
            std::string room_name;
            if (!ExtractQuotedString(line, &room_name)) {
                fclose(file);
                SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Invalid room declaration");
                return false;
            }
            scene->name = room_name;
            continue;
        }

        if (StartsWith(line, "camera ")) {
            Vec3 eye;
            Vec3 target;

            if (!ExtractVec3Property(line, "eye(", &eye) ||
                !ExtractVec3Property(line, "target(", &target)) {
                fclose(file);
                SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Camera requires eye() and target()");
                return false;
            }

            scene->camera.eye = eye;
            scene->camera.target = target;

            Vec3 up;
            if (ExtractVec3Property(line, "up(", &up)) {
                scene->camera.up = up;
            }

            float fov = 0.0f;
            if (ExtractFloatProperty(line, "fov(", &fov)) {
                scene->camera.vertical_fov_degrees = fov;
            }
            continue;
        }

        if (StartsWith(line, "spotlight")) {
            float panel_width = scene->camera_spotlight.panel_width;
            float panel_height = scene->camera_spotlight.panel_height;
            float cone_inner = scene->camera_spotlight.cone_inner_degrees;
            float cone_outer = scene->camera_spotlight.cone_outer_degrees;
            Vec3 offset = scene->camera_spotlight.local_offset;
            float range_value = scene->camera_spotlight.range;
            float intensity_value = scene->camera_spotlight.intensity;

            if (!ExtractVec2Property(line, "panel(", &panel_width, &panel_height) ||
                !ExtractVec2Property(line, "cone(", &cone_inner, &cone_outer) ||
                !ExtractVec3Property(line, "offset(", &offset) ||
                !ExtractFloatProperty(line, "range(", &range_value) ||
                !ExtractFloatProperty(line, "intensity(", &intensity_value)) {
                fclose(file);
                SetLineError(
                    error_buffer,
                    error_buffer_size,
                    scene_path,
                    line_number,
                    "Spotlight requires panel(), cone(), offset(), range(), and intensity()");
                return false;
            }

            if (panel_width <= 0.0f || panel_height <= 0.0f || range_value <= 0.0f || cone_inner <= 0.0f ||
                cone_outer <= cone_inner || intensity_value <= 0.0f) {
                fclose(file);
                SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Spotlight has invalid parameters");
                return false;
            }

            scene->camera_spotlight.enabled = true;
            scene->camera_spotlight.panel_width = panel_width;
            scene->camera_spotlight.panel_height = panel_height;
            scene->camera_spotlight.local_offset = offset;
            scene->camera_spotlight.range = range_value;
            scene->camera_spotlight.cone_inner_degrees = cone_inner;
            scene->camera_spotlight.cone_outer_degrees = cone_outer;
            scene->camera_spotlight.intensity = intensity_value;
            continue;
        }

        if (StartsWith(line, "plane ")) {
            std::string name;
            Vec3 position;
            Vec3 normal;
            float size_x = 0.0f;
            float size_y = 0.0f;
            float gray_value = 0.0f;
            float emission_value = 0.0f;

            if (!ExtractQuotedString(line, &name) ||
                !ExtractVec3Property(line, "pos(", &position) ||
                !ExtractVec3Property(line, "normal(", &normal) ||
                !ExtractVec2Property(line, "size(", &size_x, &size_y) ||
                !ExtractFloatProperty(line, "gray(", &gray_value)) {
                fclose(file);
                SetLineError(
                    error_buffer,
                    error_buffer_size,
                    scene_path,
                    line_number,
                    "Plane requires name, pos(), normal(), size(), and gray()");
                return false;
            }

            if (size_x <= 0.0f || size_y <= 0.0f || Length(normal) <= kEpsilon) {
                fclose(file);
                SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Plane has invalid size or normal");
                return false;
            }

            ExtractFloatProperty(line, "emit(", &emission_value);
            AddPlanePrimitive(scene, name, position, normal, size_x, size_y, gray_value, emission_value);
            continue;
        }

        if (StartsWith(line, "box ")) {
            std::string name;
            Vec3 position;
            Vec3 size;
            Vec3 rotation(0.0f);
            float gray_value = 0.0f;
            float emission_value = 0.0f;

            if (!ExtractQuotedString(line, &name) ||
                !ExtractVec3Property(line, "pos(", &position) ||
                !ExtractVec3Property(line, "size(", &size) ||
                !ExtractFloatProperty(line, "gray(", &gray_value)) {
                fclose(file);
                SetLineError(
                    error_buffer,
                    error_buffer_size,
                    scene_path,
                    line_number,
                    "Box requires name, pos(), size(), and gray()");
                return false;
            }

            if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
                fclose(file);
                SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Box has invalid size");
                return false;
            }

            ExtractVec3Property(line, "rot(", &rotation);
            ExtractFloatProperty(line, "emit(", &emission_value);
            AddBoxPrimitive(scene, name, position, size, rotation, gray_value, emission_value);
            continue;
        }

        fclose(file);
        SetLineError(error_buffer, error_buffer_size, scene_path, line_number, "Unknown scene directive");
        return false;
    }

    fclose(file);
    FinalizeSceneGeometry(scene);
    if (scene->triangles.empty()) {
        SetError(error_buffer, error_buffer_size, "Scene contains no renderable geometry: %s", scene_path);
        return false;
    }
    return true;
}

}  // namespace liminal
