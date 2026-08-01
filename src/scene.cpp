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
    scene->sky_background = SkyBackground();
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

static Vec3 ClampColor(const Vec3& color, float min_value, float max_value)
{
    return Vec3(
        Clamp(color.x, min_value, max_value),
        Clamp(color.y, min_value, max_value),
        Clamp(color.z, min_value, max_value));
}

static Vec3 ScaleColorToLuminance(const Vec3& tint, float target_luminance)
{
    if (target_luminance <= 0.0f) {
        return Vec3(0.0f);
    }

    const float tint_luminance = std::max(Luminance(tint), kEpsilon);
    return tint * (target_luminance / tint_luminance);
}

static MaterialSemantic InferMaterialSemantic(const std::string& name)
{
    const std::string lower = ToLowerCopy(name);
    if (lower.find("led") != std::string::npos) {
        return kMaterialSemanticRackLed;
    }

    if (lower == "ground" ||
        lower.find("desert") != std::string::npos ||
        lower.find("dune") != std::string::npos ||
        lower.find("sand") != std::string::npos ||
        lower.find("ridge") != std::string::npos ||
        lower.find("outcrop") != std::string::npos) {
        return kMaterialSemanticDesert;
    }

    return kMaterialSemanticNeutral;
}

static Vec3 BuildSemanticAlbedo(float gray_value, MaterialSemantic semantic)
{
    const float luminance = Clamp(gray_value, 0.0f, 0.95f);
    if (luminance <= 0.0f) {
        return Vec3(0.0f);
    }

    switch (semantic) {
        case kMaterialSemanticDesert:
            return ClampColor(ScaleColorToLuminance(Vec3(0.92f, 0.67f, 0.24f), luminance), 0.0f, 0.95f);
        case kMaterialSemanticRackLed:
            return ClampColor(ScaleColorToLuminance(Vec3(0.95f, 0.18f, 0.14f), luminance), 0.0f, 0.95f);
        case kMaterialSemanticNeutral:
        default:
            return Vec3(luminance);
    }
}

static Vec3 BuildSemanticEmission(float emission_value, MaterialSemantic semantic)
{
    const float intensity = std::max(0.0f, emission_value);
    if (intensity <= 0.0f) {
        return Vec3(0.0f);
    }

    switch (semantic) {
        case kMaterialSemanticRackLed:
            return ScaleColorToLuminance(Vec3(1.00f, 0.12f, 0.08f), intensity);
        case kMaterialSemanticNeutral:
        case kMaterialSemanticDesert:
        default:
            return Vec3(intensity);
    }
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

static bool ParseUnsignedText(const char* text, unsigned int* value)
{
    if (!text || !value) {
        return false;
    }

    char* end = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (end == text) {
        return false;
    }

    *value = static_cast<unsigned int>(parsed);
    return true;
}

static float LerpScalar(float a, float b, float t)
{
    return a + (b - a) * t;
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

static bool ExtractUnsignedProperty(const std::string& line, const char* key, unsigned int* value)
{
    std::string text;
    return ExtractPropertyText(line, key, &text) && ParseUnsignedText(text.c_str(), value);
}

static Material FinalizeMaterial(const MaterialDraft& draft)
{
    Material material;
    material.name = draft.name;
    material.semantic = InferMaterialSemantic(draft.name);

    const Vec3 kd = draft.has_kd ? draft.kd : Vec3(0.6f);
    const Vec3 ka = draft.has_ka ? draft.ka : kd;

    const float kd_luminance = Luminance(kd);
    const float ka_luminance = Luminance(ka);
    const bool emissive = (draft.name == "light") || kd_luminance > 1.0f || ka_luminance > 1.0f;

    if (emissive) {
        material.albedo = Vec3(0.0f);
        material.emission = BuildSemanticEmission(std::max(kd_luminance, ka_luminance), material.semantic);
    } else {
        material.albedo = BuildSemanticAlbedo(
            Clamp(kd_luminance > 0.0f ? kd_luminance : ka_luminance, 0.05f, 0.95f),
            material.semantic);
        material.emission = Vec3(0.0f);
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
    material.semantic = InferMaterialSemantic(name);
    material.albedo = BuildSemanticAlbedo(gray_value, material.semantic);
    material.emission = BuildSemanticEmission(emission_value, material.semantic);
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

static std::string PrefabChildName(const std::string& base_name, const char* suffix)
{
    return base_name + "_" + suffix;
}

static void AddPrefabChildBox(
    Scene* scene,
    const std::string& base_name,
    const char* suffix,
    const Vec3& prefab_center,
    const Vec3& local_center,
    const Vec3& size,
    float gray_value,
    float emission_value = 0.0f)
{
    AddBoxPrimitive(
        scene,
        PrefabChildName(base_name, suffix),
        prefab_center + local_center,
        size,
        Vec3(0.0f),
        gray_value,
        emission_value);
}

static void AddGatePrefab(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& size,
    float frame_gray,
    float detail_gray,
    unsigned int bars)
{
    const unsigned int bar_count = std::max(1u, bars);
    const float pillar_width = Clamp(size.x * 0.10f, 0.10f, size.x * 0.22f);
    const float top_beam_height = Clamp(size.y * 0.10f, 0.10f, size.y * 0.20f);
    const float bottom_beam_height = Clamp(size.y * 0.08f, 0.08f, size.y * 0.16f);
    const float frame_depth = std::max(size.z, 0.08f);
    const float bar_depth = std::max(frame_depth * 0.35f, 0.05f);

    AddPrefabChildBox(
        scene,
        name,
        "frame_left",
        center,
        Vec3(-(size.x - pillar_width) * 0.5f, 0.0f, 0.0f),
        Vec3(pillar_width, size.y, frame_depth),
        frame_gray);
    AddPrefabChildBox(
        scene,
        name,
        "frame_right",
        center,
        Vec3((size.x - pillar_width) * 0.5f, 0.0f, 0.0f),
        Vec3(pillar_width, size.y, frame_depth),
        frame_gray);
    AddPrefabChildBox(
        scene,
        name,
        "frame_top",
        center,
        Vec3(0.0f, (size.y - top_beam_height) * 0.5f, 0.0f),
        Vec3(size.x, top_beam_height, frame_depth),
        frame_gray);
    AddPrefabChildBox(
        scene,
        name,
        "frame_bottom",
        center,
        Vec3(0.0f, -(size.y - bottom_beam_height) * 0.5f, 0.0f),
        Vec3(size.x, bottom_beam_height, frame_depth),
        frame_gray);

    const float bar_clear_width = std::max(size.x - pillar_width * 2.0f - 0.16f, 0.20f);
    const float bar_height = std::max(size.y - top_beam_height - bottom_beam_height - 0.16f, 0.20f);
    const float bar_center_y = (bottom_beam_height - top_beam_height) * 0.5f;
    const float bar_width = Clamp(bar_clear_width / (static_cast<float>(bar_count) * 4.5f), 0.05f, 0.14f);

    for (unsigned int bar_index = 0; bar_index < bar_count; ++bar_index) {
        float x = 0.0f;
        if (bar_count > 1) {
            const float t = static_cast<float>(bar_index) / static_cast<float>(bar_count - 1);
            x = LerpScalar(-bar_clear_width * 0.5f, bar_clear_width * 0.5f, t);
        }

        char suffix[32];
        snprintf(suffix, sizeof(suffix), "bar_%02u", bar_index + 1u);
        AddPrefabChildBox(
            scene,
            name,
            suffix,
            center,
            Vec3(x, bar_center_y, 0.0f),
            Vec3(bar_width, bar_height, bar_depth),
            detail_gray);
    }
}

static void AddRackPrefab(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& size,
    float body_gray,
    float detail_gray)
{
    const float core_gray = Clamp(body_gray * 0.72f, 0.0f, 0.95f);
    const float frame_width = Clamp(size.x * 0.11f, 0.05f, size.x * 0.24f);
    const float frame_height = Clamp(size.y * 0.06f, 0.05f, size.y * 0.16f);
    const float front_depth = std::max(size.z * 0.12f, 0.05f);
    const float core_depth = std::max(size.z * 0.88f, 0.12f);
    const float core_height = std::max(size.y * 0.94f, 0.20f);
    const float core_width = std::max(size.x * 0.82f, 0.16f);
    const float front_z = -(size.z - front_depth) * 0.5f;

    AddPrefabChildBox(scene, name, "core", center, Vec3(0.0f, 0.0f, 0.02f * size.z), Vec3(core_width, core_height, core_depth), core_gray);
    AddPrefabChildBox(scene, name, "frame_left", center, Vec3(-(size.x - frame_width) * 0.5f, 0.0f, front_z), Vec3(frame_width, size.y, front_depth), detail_gray);
    AddPrefabChildBox(scene, name, "frame_right", center, Vec3((size.x - frame_width) * 0.5f, 0.0f, front_z), Vec3(frame_width, size.y, front_depth), detail_gray);
    AddPrefabChildBox(scene, name, "frame_top", center, Vec3(0.0f, (size.y - frame_height) * 0.5f, front_z), Vec3(size.x, frame_height, front_depth), detail_gray);
    AddPrefabChildBox(scene, name, "frame_bottom", center, Vec3(0.0f, -(size.y - frame_height) * 0.5f, front_z), Vec3(size.x, frame_height, front_depth), detail_gray);

    for (int band_index = 0; band_index < 4; ++band_index) {
        const float t = static_cast<float>(band_index) / 3.0f;
        const float y = LerpScalar(-size.y * 0.22f, size.y * 0.22f, t);
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "slot_%02d", band_index + 1);
        AddPrefabChildBox(
            scene,
            name,
            suffix,
            center,
            Vec3(0.0f, y, front_z),
            Vec3(std::max(size.x - frame_width * 2.4f, 0.10f), std::max(frame_height * 0.45f, 0.04f), front_depth * 0.65f),
            body_gray);
    }

    const float led_width = Clamp(size.x * 0.05f, 0.03f, 0.08f);
    const float led_height = Clamp(size.y * 0.03f, 0.03f, 0.08f);
    const float led_depth = std::max(front_depth * 0.40f, 0.03f);
    const float led_x = -(size.x * 0.5f) + frame_width + led_width * 1.6f;

    for (int led_index = 0; led_index < 4; ++led_index) {
        const float t = static_cast<float>(led_index) / 3.0f;
        const float y = LerpScalar(-size.y * 0.18f, size.y * 0.18f, t);
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "led_%02d", led_index + 1);
        AddPrefabChildBox(
            scene,
            name,
            suffix,
            center,
            Vec3(led_x, y, front_z - front_depth * 0.08f),
            Vec3(led_width, led_height, led_depth),
            0.05f,
            4.0f);
    }
}

static void AddCratePrefab(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& size,
    float body_gray,
    float detail_gray)
{
    const float min_dimension = std::min(std::min(size.x, size.y), size.z);
    const float lid_thickness = Clamp(min_dimension * 0.12f, 0.04f, min_dimension * 0.22f);
    const float body_height = std::max(size.y - lid_thickness, lid_thickness);
    const Vec3 body_size(size.x, body_height, size.z);
    const Vec3 lid_size(
        std::max(size.x * 1.04f, size.x + 0.03f),
        lid_thickness,
        std::max(size.z * 1.04f, size.z + 0.03f));
    const float lid_center_y = body_size.y * 0.5f;

    AddPrefabChildBox(scene, name, "body", center, Vec3(0.0f, -lid_thickness * 0.5f, 0.0f), body_size, body_gray);
    AddPrefabChildBox(scene, name, "lid", center, Vec3(0.0f, lid_center_y, 0.0f), lid_size, detail_gray);
}

static void AddCoolingUnitPrefab(
    Scene* scene,
    const std::string& name,
    const Vec3& center,
    const Vec3& size,
    float body_gray,
    float detail_gray)
{
    const float cap_height = Clamp(size.y * 0.10f, 0.05f, size.y * 0.22f);
    const float plinth_height = Clamp(size.y * 0.08f, 0.04f, size.y * 0.16f);
    const float vent_depth = std::max(size.z * 0.12f, 0.05f);
    const float vent_width = std::max(size.x * 0.68f, 0.10f);
    const float vent_front_z = -(size.z - vent_depth) * 0.5f;

    AddPrefabChildBox(scene, name, "body", center, Vec3(0.0f, 0.0f, 0.0f), Vec3(size.x * 0.90f, size.y * 0.90f, size.z * 0.92f), body_gray);
    AddPrefabChildBox(scene, name, "cap", center, Vec3(0.0f, (size.y - cap_height) * 0.5f, 0.0f), Vec3(size.x, cap_height, size.z), detail_gray);
    AddPrefabChildBox(scene, name, "plinth", center, Vec3(0.0f, -(size.y - plinth_height) * 0.5f, 0.0f), Vec3(size.x * 0.94f, plinth_height, size.z * 0.94f), detail_gray);

    for (int vent_index = 0; vent_index < 4; ++vent_index) {
        const float t = static_cast<float>(vent_index) / 3.0f;
        const float y = LerpScalar(-size.y * 0.22f, size.y * 0.22f, t);
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "vent_%02d", vent_index + 1);
        AddPrefabChildBox(
            scene,
            name,
            suffix,
            center,
            Vec3(0.0f, y, vent_front_z),
            Vec3(vent_width, std::max(size.y * 0.06f, 0.04f), vent_depth),
            detail_gray);
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
        if (!IsNearBlack(material.emission) && material.semantic != kMaterialSemanticRackLed) {
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

static bool ParseSceneV1Directive(
    const std::string& line,
    const char* scene_name,
    int line_number,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (StartsWith(line, "room ")) {
        std::string room_name;
        if (!ExtractQuotedString(line, &room_name)) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Invalid room declaration");
            return false;
        }
        scene->name = room_name;
        return true;
    }

    if (StartsWith(line, "camera ")) {
        Vec3 eye;
        Vec3 target;

        if (!ExtractVec3Property(line, "eye(", &eye) ||
            !ExtractVec3Property(line, "target(", &target)) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Camera requires eye() and target()");
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
        return true;
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
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "Spotlight requires panel(), cone(), offset(), range(), and intensity()");
            return false;
        }

        if (panel_width <= 0.0f || panel_height <= 0.0f || range_value <= 0.0f || cone_inner <= 0.0f ||
            cone_outer <= cone_inner || intensity_value <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Spotlight has invalid parameters");
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
        return true;
    }

    if (StartsWith(line, "sky ")) {
        float zenith_luminance = scene->sky_background.zenith_luminance;
        float horizon_luminance = scene->sky_background.horizon_luminance;
        float nadir_luminance = scene->sky_background.nadir_luminance;
        float horizon_band = scene->sky_background.horizon_band;
        float horizon_curve = scene->sky_background.horizon_curve;
        float noise_amount = scene->sky_background.noise_amount;
        Vec3 stars(
            scene->sky_background.star_density,
            scene->sky_background.star_intensity,
            scene->sky_background.star_radius);
        unsigned int seed = scene->sky_background.seed;

        if (!ExtractFloatProperty(line, "zenith(", &zenith_luminance) ||
            !ExtractFloatProperty(line, "horizon(", &horizon_luminance) ||
            !ExtractFloatProperty(line, "nadir(", &nadir_luminance) ||
            !ExtractFloatProperty(line, "band(", &horizon_band) ||
            !ExtractFloatProperty(line, "curve(", &horizon_curve) ||
            !ExtractFloatProperty(line, "noise(", &noise_amount)) {
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "Sky requires zenith(), horizon(), nadir(), band(), curve(), and noise()");
            return false;
        }

        ExtractVec3Property(line, "stars(", &stars);
        ExtractUnsignedProperty(line, "seed(", &seed);

        if (zenith_luminance < 0.0f || horizon_luminance < 0.0f || nadir_luminance < 0.0f ||
            horizon_band <= 0.0f || horizon_band > 1.0f || horizon_curve <= 0.0f || noise_amount < 0.0f ||
            stars.x < 0.0f || stars.y < 0.0f || stars.z < 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Sky has invalid parameters");
            return false;
        }

        scene->sky_background.enabled = true;
        scene->sky_background.zenith_luminance = zenith_luminance;
        scene->sky_background.horizon_luminance = horizon_luminance;
        scene->sky_background.nadir_luminance = nadir_luminance;
        scene->sky_background.horizon_band = horizon_band;
        scene->sky_background.horizon_curve = horizon_curve;
        scene->sky_background.noise_amount = noise_amount;
        scene->sky_background.star_density = stars.x;
        scene->sky_background.star_intensity = stars.y;
        scene->sky_background.star_radius = stars.z;
        scene->sky_background.seed = seed;
        return true;
    }

    if (StartsWith(line, "prefab_gate ")) {
        std::string name;
        Vec3 position;
        Vec3 size;
        float gray_value = 0.0f;
        float detail_value = 0.0f;
        unsigned int bars = 5u;

        if (!ExtractQuotedString(line, &name) ||
            !ExtractVec3Property(line, "pos(", &position) ||
            !ExtractVec3Property(line, "size(", &size) ||
            !ExtractFloatProperty(line, "gray(", &gray_value)) {
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "prefab_gate requires name, pos(), size(), and gray()");
            return false;
        }

        if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "prefab_gate has invalid size");
            return false;
        }

        if (!ExtractFloatProperty(line, "detail(", &detail_value)) {
            detail_value = Clamp(gray_value + 0.12f, 0.0f, 0.95f);
        }
        ExtractUnsignedProperty(line, "bars(", &bars);
        AddGatePrefab(scene, name, position, size, gray_value, detail_value, bars);
        return true;
    }

    if (StartsWith(line, "prefab_rack ")) {
        std::string name;
        Vec3 position;
        Vec3 size;
        float gray_value = 0.0f;
        float detail_value = 0.0f;

        if (!ExtractQuotedString(line, &name) ||
            !ExtractVec3Property(line, "pos(", &position) ||
            !ExtractVec3Property(line, "size(", &size) ||
            !ExtractFloatProperty(line, "gray(", &gray_value)) {
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "prefab_rack requires name, pos(), size(), and gray()");
            return false;
        }

        if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "prefab_rack has invalid size");
            return false;
        }

        if (!ExtractFloatProperty(line, "detail(", &detail_value)) {
            detail_value = Clamp(gray_value + 0.16f, 0.0f, 0.95f);
        }
        AddRackPrefab(scene, name, position, size, gray_value, detail_value);
        return true;
    }

    if (StartsWith(line, "prefab_crate ")) {
        std::string name;
        Vec3 position;
        Vec3 size;
        float gray_value = 0.0f;
        float detail_value = 0.0f;

        if (!ExtractQuotedString(line, &name) ||
            !ExtractVec3Property(line, "pos(", &position) ||
            !ExtractVec3Property(line, "size(", &size) ||
            !ExtractFloatProperty(line, "gray(", &gray_value)) {
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "prefab_crate requires name, pos(), size(), and gray()");
            return false;
        }

        if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "prefab_crate has invalid size");
            return false;
        }

        if (!ExtractFloatProperty(line, "detail(", &detail_value)) {
            detail_value = Clamp(gray_value + 0.12f, 0.0f, 0.95f);
        }
        AddCratePrefab(scene, name, position, size, gray_value, detail_value);
        return true;
    }

    if (StartsWith(line, "prefab_cooling_unit ")) {
        std::string name;
        Vec3 position;
        Vec3 size;
        float gray_value = 0.0f;
        float detail_value = 0.0f;

        if (!ExtractQuotedString(line, &name) ||
            !ExtractVec3Property(line, "pos(", &position) ||
            !ExtractVec3Property(line, "size(", &size) ||
            !ExtractFloatProperty(line, "gray(", &gray_value)) {
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "prefab_cooling_unit requires name, pos(), size(), and gray()");
            return false;
        }

        if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "prefab_cooling_unit has invalid size");
            return false;
        }

        if (!ExtractFloatProperty(line, "detail(", &detail_value)) {
            detail_value = Clamp(gray_value + 0.11f, 0.0f, 0.95f);
        }
        AddCoolingUnitPrefab(scene, name, position, size, gray_value, detail_value);
        return true;
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
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "Plane requires name, pos(), normal(), size(), and gray()");
            return false;
        }

        if (size_x <= 0.0f || size_y <= 0.0f || Length(normal) <= kEpsilon) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Plane has invalid size or normal");
            return false;
        }

        ExtractFloatProperty(line, "emit(", &emission_value);
        AddPlanePrimitive(scene, name, position, normal, size_x, size_y, gray_value, emission_value);
        return true;
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
            SetLineError(
                error_buffer,
                error_buffer_size,
                scene_name,
                line_number,
                "Box requires name, pos(), size(), and gray()");
            return false;
        }

        if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
            SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Box has invalid size");
            return false;
        }

        ExtractVec3Property(line, "rot(", &rotation);
        ExtractFloatProperty(line, "emit(", &emission_value);
        AddBoxPrimitive(scene, name, position, size, rotation, gray_value, emission_value);
        return true;
    }

    SetLineError(error_buffer, error_buffer_size, scene_name, line_number, "Unknown scene directive");
    return false;
}

static bool FinalizeSceneLoad(const char* scene_name, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    FinalizeSceneGeometry(scene);
    if (scene->triangles.empty()) {
        SetError(error_buffer, error_buffer_size, "Scene contains no renderable geometry: %s", scene_name ? scene_name : "(scene)");
        return false;
    }
    return true;
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
    default_material.albedo = Vec3(0.6f);
    default_material.emission = Vec3(0.0f);
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
    if (IsNearBlack(light_material.emission)) {
        light_material.albedo = Vec3(0.0f);
        light_material.emission = Vec3(12.0f);
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
        if (!ParseSceneV1Directive(line, scene_path, line_number, scene, error_buffer, error_buffer_size)) {
            fclose(file);
            return false;
        }
    }

    fclose(file);
    return FinalizeSceneLoad(scene_path, scene, error_buffer, error_buffer_size);
}

bool LoadSceneFromSceneText(
    const char* scene_name,
    const char* scene_text,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene_name || !scene_text || !scene) {
        SetError(error_buffer, error_buffer_size, "Invalid scene text source: %s", scene_name ? scene_name : "(null)");
        return false;
    }

    ResetScene(scene);
    scene->name = BasenameOf(scene_name);

    const char* cursor = scene_text;
    int line_number = 0;
    while (*cursor) {
        const char* line_start = cursor;
        while (*cursor && *cursor != '\n') {
            ++cursor;
        }

        std::string raw_line(line_start, cursor - line_start);
        if (*cursor == '\n') {
            ++cursor;
        }

        ++line_number;
        std::string line = Trim(StripComment(raw_line));
        if (line.empty()) {
            continue;
        }

        if (!ParseSceneV1Directive(line, scene_name, line_number, scene, error_buffer, error_buffer_size)) {
            return false;
        }
    }

    return FinalizeSceneLoad(scene_name, scene, error_buffer, error_buffer_size);
}

}  // namespace liminal
