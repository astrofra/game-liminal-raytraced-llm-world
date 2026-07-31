#include "renderer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace liminal {

namespace {

struct CameraBasis {
    Vec3 forward;
    Vec3 right;
    Vec3 up;
};

struct CameraLightState {
    bool enabled;
    Vec3 center;
    Vec3 normal;
    Vec3 tangent;
    Vec3 bitangent;
    float panel_width;
    float panel_height;
    float area;
    float range;
    float cone_inner_cos;
    float cone_outer_cos;
    float intensity;

    CameraLightState()
        : enabled(false)
        , center(0.0f)
        , normal(0.0f, 0.0f, 1.0f)
        , tangent(1.0f, 0.0f, 0.0f)
        , bitangent(0.0f, 1.0f, 0.0f)
        , panel_width(1.0f)
        , panel_height(1.0f)
        , area(1.0f)
        , range(12.0f)
        , cone_inner_cos(1.0f)
        , cone_outer_cos(0.0f)
        , intensity(120.0f)
    {
    }
};

static const float kSampleRadianceClamp = 6.0f;

static float Saturate(float value)
{
    return Clamp(value, 0.0f, 1.0f);
}

static float Smoothstep(float edge0, float edge1, float value)
{
    if (fabsf(edge1 - edge0) <= kEpsilon) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = Saturate((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float Fract(float value)
{
    return value - floorf(value);
}

static uint32_t HashU32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static uint32_t HashCombine3(uint32_t a, uint32_t b, uint32_t c)
{
    return HashU32(a ^ (HashU32(b) << 1) ^ (HashU32(c) << 7));
}

static float HashToUnitFloat(uint32_t value)
{
    const uint32_t bits = value >> 8;
    return static_cast<float>(bits) / static_cast<float>(0x01000000u);
}

static std::string ToLowerCopy(const char* text)
{
    std::string lower = text ? text : "";
    for (size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] >= 'A' && lower[index] <= 'Z') {
            lower[index] = static_cast<char>(lower[index] - 'A' + 'a');
        }
    }
    return lower;
}

static std::string ExtensionOf(const char* path)
{
    if (!path) {
        return std::string();
    }

    const std::string lower = ToLowerCopy(path);
    const size_t dot = lower.find_last_of('.');
    return dot == std::string::npos ? std::string() : lower.substr(dot);
}

static CameraBasis BuildCameraBasis(const Camera& camera)
{
    CameraBasis basis;
    basis.forward = Normalize(camera.target - camera.eye);
    basis.right = Normalize(Cross(basis.forward, camera.up));
    basis.up = Normalize(Cross(basis.right, basis.forward));
    return basis;
}

static CameraLightState BuildCameraLightState(
    const Scene& scene,
    const CameraBasis& basis)
{
    CameraLightState state;
    if (!scene.camera_spotlight.enabled) {
        return state;
    }

    state.enabled = true;
    state.normal = basis.forward;
    state.tangent = basis.right;
    state.bitangent = basis.up;
    state.panel_width = scene.camera_spotlight.panel_width;
    state.panel_height = scene.camera_spotlight.panel_height;
    state.area = state.panel_width * state.panel_height;
    state.range = scene.camera_spotlight.range;
    state.intensity = scene.camera_spotlight.intensity;
    state.center = scene.camera.eye +
        basis.right * scene.camera_spotlight.local_offset.x +
        basis.up * scene.camera_spotlight.local_offset.y +
        basis.forward * scene.camera_spotlight.local_offset.z;

    const float to_radians = kPi / 180.0f;
    state.cone_inner_cos = cosf(scene.camera_spotlight.cone_inner_degrees * to_radians);
    state.cone_outer_cos = cosf(scene.camera_spotlight.cone_outer_degrees * to_radians);
    return state;
}

static float EvaluateSkyStars(const SkyBackground& sky, const Vec3& direction)
{
    if (sky.star_density <= 0.0f || sky.star_intensity <= 0.0f || sky.star_radius <= 0.0f || direction.y <= 0.0f) {
        return 0.0f;
    }

    const float phi = atan2f(direction.z, direction.x);
    const float wrapped_phi = phi < 0.0f ? phi + 2.0f * kPi : phi;
    const float theta = acosf(Clamp(direction.y, -1.0f, 1.0f));
    const float u = wrapped_phi / (2.0f * kPi);
    const float v = theta / kPi;

    const float grid_x = u * 768.0f;
    const float grid_y = v * 384.0f;
    const uint32_t cell_x = static_cast<uint32_t>(floorf(grid_x));
    const uint32_t cell_y = static_cast<uint32_t>(floorf(grid_y));

    const uint32_t cell_hash = HashCombine3(cell_x, cell_y, sky.seed);
    if (HashToUnitFloat(cell_hash) > sky.star_density) {
        return 0.0f;
    }

    const float local_x = Fract(grid_x);
    const float local_y = Fract(grid_y);
    const float star_x = HashToUnitFloat(HashCombine3(cell_x, cell_y, sky.seed + 1u));
    const float star_y = HashToUnitFloat(HashCombine3(cell_x, cell_y, sky.seed + 2u));
    const float dx = local_x - star_x;
    const float dy = local_y - star_y;
    const float radius = std::max(sky.star_radius, 0.0001f);
    const float distance = sqrtf(dx * dx + dy * dy);
    if (distance >= radius) {
        return 0.0f;
    }

    const float shape = 1.0f - distance / radius;
    const float sparkle = HashToUnitFloat(HashCombine3(cell_x, cell_y, sky.seed + 3u));
    const float intensity = sky.star_intensity * (0.75f + 0.5f * sparkle);
    const float altitude_visibility = Smoothstep(0.02f, 0.24f, direction.y);
    const float core = powf(shape, 12.0f);
    const float halo = powf(shape, 4.0f) * 0.28f;
    return intensity * altitude_visibility * (core + halo);
}

static float SampleSkyBackground(const SkyBackground& sky, const Vec3& direction, Rng* rng)
{
    if (!sky.enabled) {
        return 0.0f;
    }

    const float abs_y = fabsf(direction.y);
    const float horizon_t = 1.0f - Saturate(abs_y / std::max(sky.horizon_band, 0.0001f));
    const float curved_horizon = powf(horizon_t, sky.horizon_curve);
    const float base_luminance = direction.y >= 0.0f ? sky.zenith_luminance : sky.nadir_luminance;

    float luminance = Lerp(base_luminance, sky.horizon_luminance, curved_horizon);
    luminance += EvaluateSkyStars(sky, direction);

    if (sky.noise_amount > 0.0f && rng) {
        const float phi = atan2f(direction.z, direction.x);
        const float wrapped_phi = phi < 0.0f ? phi + 2.0f * kPi : phi;
        const float theta = acosf(Clamp(direction.y, -1.0f, 1.0f));
        const uint32_t noise_x = static_cast<uint32_t>(floorf((wrapped_phi / (2.0f * kPi)) * 4096.0f));
        const uint32_t noise_y = static_cast<uint32_t>(floorf((theta / kPi) * 2048.0f));
        const float directional_noise =
            HashToUnitFloat(HashCombine3(noise_x, noise_y, sky.seed + 11u)) * 2.0f - 1.0f;
        const float sample_noise = rng->NextFloat() * 2.0f - 1.0f;
        const float noise_weight = 0.35f + 0.65f * curved_horizon;
        const float combined_noise = directional_noise * 0.20f + sample_noise * 0.80f;
        luminance += combined_noise * sky.noise_amount * noise_weight;
    }

    return std::max(0.0f, luminance);
}

// Rewritten from the slab-style rejection logic in the 2003 raytracer.
static bool IntersectAabb(const Ray& ray, const Aabb& bounds, float max_distance)
{
    float t_min = kEpsilon;
    float t_max = max_distance;

    for (int axis = 0; axis < 3; ++axis) {
        const float origin = axis == 0 ? ray.origin.x : (axis == 1 ? ray.origin.y : ray.origin.z);
        const float direction = axis == 0 ? ray.direction.x : (axis == 1 ? ray.direction.y : ray.direction.z);
        const float min_bound = axis == 0 ? bounds.min.x : (axis == 1 ? bounds.min.y : bounds.min.z);
        const float max_bound = axis == 0 ? bounds.max.x : (axis == 1 ? bounds.max.y : bounds.max.z);

        if (fabsf(direction) <= kEpsilon) {
            if (origin < min_bound || origin > max_bound) {
                return false;
            }
            continue;
        }

        const float inverse_direction = 1.0f / direction;
        float t0 = (min_bound - origin) * inverse_direction;
        float t1 = (max_bound - origin) * inverse_direction;
        if (t0 > t1) {
            const float temp = t0;
            t0 = t1;
            t1 = temp;
        }

        t_min = std::max(t_min, t0);
        t_max = std::min(t_max, t1);
        if (t_max <= t_min) {
            return false;
        }
    }

    return true;
}

// Rewritten from the Moller-Trumbore code path used in the 2003 raytracer.
static bool IntersectTriangle(const Ray& ray, const Triangle& triangle, float max_distance, Hit* hit)
{
    const Vec3 edge1 = triangle.v1 - triangle.v0;
    const Vec3 edge2 = triangle.v2 - triangle.v0;
    const Vec3 pvec = Cross(ray.direction, edge2);
    const float determinant = Dot(edge1, pvec);

    if (determinant > -kEpsilon && determinant < kEpsilon) {
        return false;
    }

    const float inverse_determinant = 1.0f / determinant;
    const Vec3 tvec = ray.origin - triangle.v0;
    const float u = Dot(tvec, pvec) * inverse_determinant;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vec3 qvec = Cross(tvec, edge1);
    const float v = Dot(ray.direction, qvec) * inverse_determinant;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    const float t = Dot(edge2, qvec) * inverse_determinant;
    if (t <= kEpsilon || t >= max_distance || t >= hit->t) {
        return false;
    }

    hit->t = t;
    hit->position = ray.origin + ray.direction * t;
    hit->normal = triangle.normal;
    if (Dot(hit->normal, ray.direction) > 0.0f) {
        hit->normal = -hit->normal;
    }

    return true;
}

static bool IntersectScene(const Scene& scene, const Ray& ray, float max_distance, Hit* hit)
{
    if (scene.bvh_nodes.empty()) {
        return false;
    }

    int stack[128];
    int stack_size = 0;
    stack[stack_size++] = 0;

    bool found_hit = false;
    while (stack_size > 0) {
        const int node_index = stack[--stack_size];
        const BvhNode& node = scene.bvh_nodes[node_index];

        if (!IntersectAabb(ray, node.bounds, std::min(max_distance, hit->t))) {
            continue;
        }

        if (node.IsLeaf()) {
            for (int index = 0; index < node.triangle_count; ++index) {
                const int triangle_index = node.first_triangle + index;
                if (IntersectTriangle(ray, scene.triangles[triangle_index], max_distance, hit)) {
                    hit->triangle_index = triangle_index;
                    found_hit = true;
                }
            }
            continue;
        }

        if (node.left >= 0) {
            stack[stack_size++] = node.left;
        }
        if (node.right >= 0) {
            stack[stack_size++] = node.right;
        }
    }

    return found_hit;
}

static bool IsOccluded(const Scene& scene, const Ray& ray, float max_distance)
{
    Hit hit;
    hit.t = max_distance;
    return IntersectScene(scene, ray, max_distance, &hit);
}

static Vec3 SampleCosineHemisphere(const Vec3& normal, Rng* rng)
{
    const float u1 = rng->NextFloat();
    const float u2 = rng->NextFloat();

    const float radius = sqrtf(u1);
    const float angle = 2.0f * kPi * u2;

    const float x = radius * cosf(angle);
    const float y = radius * sinf(angle);
    const float z = sqrtf(std::max(0.0f, 1.0f - u1));

    const Vec3 tangent_seed = fabsf(normal.y) < 0.999f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 tangent = Normalize(Cross(tangent_seed, normal));
    const Vec3 bitangent = Cross(normal, tangent);

    return Normalize(tangent * x + bitangent * y + normal * z);
}

static Vec3 SamplePointOnTriangle(const Triangle& triangle, Rng* rng)
{
    float u = rng->NextFloat();
    float v = rng->NextFloat();
    if (u + v > 1.0f) {
        u = 1.0f - u;
        v = 1.0f - v;
    }

    return triangle.v0 + (triangle.v1 - triangle.v0) * u + (triangle.v2 - triangle.v0) * v;
}

static Vec3 SamplePointOnCameraLight(const CameraLightState& light, Rng* rng)
{
    const float u = rng->NextFloat() - 0.5f;
    const float v = rng->NextFloat() - 0.5f;
    return light.center +
        light.tangent * (u * light.panel_width) +
        light.bitangent * (v * light.panel_height);
}

static float EstimateEmissiveTriangleLighting(
    const Scene& scene,
    const Hit& hit,
    const Material& material,
    int light_samples,
    Rng* rng)
{
    if (scene.emissive_triangles.empty() || material.albedo <= 0.0f || light_samples <= 0) {
        return 0.0f;
    }

    float contribution = 0.0f;
    const float light_count = static_cast<float>(scene.emissive_triangles.size());

    for (int sample_index = 0; sample_index < light_samples; ++sample_index) {
        const int light_slot = static_cast<int>(rng->NextFloat() * light_count);
        const int clamped_slot = std::min(static_cast<int>(light_count) - 1, light_slot);
        const Triangle& light = scene.triangles[scene.emissive_triangles[clamped_slot]];
        const Material& light_material = scene.materials[light.material_index];

        const Vec3 sample_point = SamplePointOnTriangle(light, rng);
        const Vec3 to_light = sample_point - hit.position;
        const float distance_squared = LengthSquared(to_light);
        if (distance_squared <= kEpsilon) {
            continue;
        }

        const float distance = sqrtf(distance_squared);
        const Vec3 direction = to_light / distance;

        const float cos_surface = Dot(hit.normal, direction);
        const float cos_light = Dot(light.normal, -direction);
        if (cos_surface <= 0.0f || cos_light <= 0.0f) {
            continue;
        }

        Ray shadow_ray;
        shadow_ray.origin = hit.position + hit.normal * 0.001f;
        shadow_ray.direction = direction;

        if (IsOccluded(scene, shadow_ray, distance - 0.002f)) {
            continue;
        }

        const float area_pdf = 1.0f / light.area;
        const float selection_pdf = 1.0f / light_count;
        const float pdf = area_pdf * selection_pdf;
        const float brdf = material.albedo / kPi;

        contribution += brdf * light_material.emission * cos_surface * cos_light / (distance_squared * pdf);
    }

    return contribution / static_cast<float>(light_samples);
}

static float EstimateCameraSpotLighting(
    const Scene& scene,
    const CameraLightState& camera_light,
    const Hit& hit,
    const Material& material,
    int light_samples,
    Rng* rng)
{
    if (!camera_light.enabled || material.albedo <= 0.0f || light_samples <= 0) {
        return 0.0f;
    }

    float contribution = 0.0f;
    const float area_pdf = 1.0f / std::max(camera_light.area, kEpsilon);
    const float brdf = material.albedo / kPi;

    for (int sample_index = 0; sample_index < light_samples; ++sample_index) {
        const Vec3 sample_point = SamplePointOnCameraLight(camera_light, rng);
        const Vec3 to_light = sample_point - hit.position;
        const float distance_squared = LengthSquared(to_light);
        if (distance_squared <= kEpsilon) {
            continue;
        }

        const float distance = sqrtf(distance_squared);
        if (distance >= camera_light.range) {
            continue;
        }

        const Vec3 direction = to_light / distance;
        const Vec3 light_to_surface = -direction;

        const float cos_surface = Dot(hit.normal, direction);
        const float cos_light = Dot(camera_light.normal, light_to_surface);
        if (cos_surface <= 0.0f || cos_light <= 0.0f) {
            continue;
        }

        const float cone_factor =
            1.0f - Smoothstep(camera_light.cone_inner_cos, camera_light.cone_outer_cos, cos_light);
        const float range_factor = 1.0f - Smoothstep(camera_light.range * 0.55f, camera_light.range, distance);
        if (cone_factor <= 0.0f || range_factor <= 0.0f) {
            continue;
        }

        Ray shadow_ray;
        shadow_ray.origin = hit.position + hit.normal * 0.001f;
        shadow_ray.direction = direction;
        if (IsOccluded(scene, shadow_ray, distance - 0.002f)) {
            continue;
        }

        contribution += brdf * camera_light.intensity * cone_factor * range_factor * cos_surface * cos_light /
            (distance_squared * area_pdf);
    }

    return contribution / static_cast<float>(light_samples);
}

static float EstimateDirectLighting(
    const Scene& scene,
    const CameraLightState& camera_light,
    const Hit& hit,
    const Material& material,
    int light_samples,
    Rng* rng)
{
    return EstimateEmissiveTriangleLighting(scene, hit, material, light_samples, rng) +
        EstimateCameraSpotLighting(scene, camera_light, hit, material, light_samples, rng);
}

static float TracePath(
    const Scene& scene,
    const CameraLightState& camera_light,
    const RenderConfig& config,
    const Ray& camera_ray,
    Rng* rng)
{
    Ray ray = camera_ray;
    float throughput = 1.0f;
    float radiance = 0.0f;

    for (int bounce = 0; bounce < config.max_bounces; ++bounce) {
        Hit hit;
        if (!IntersectScene(scene, ray, kHuge, &hit)) {
            radiance += throughput * SampleSkyBackground(scene.sky_background, ray.direction, rng);
            break;
        }

        const Triangle& triangle = scene.triangles[hit.triangle_index];
        const Material& material = scene.materials[triangle.material_index];

        if (material.emission > 0.0f) {
            radiance += throughput * material.emission;
            break;
        }

        radiance += throughput *
            EstimateDirectLighting(scene, camera_light, hit, material, config.direct_light_samples, rng);

        if (material.albedo <= 0.0f) {
            break;
        }

        throughput *= material.albedo;
        if (bounce >= 1) {
            const float survival_probability = Clamp(throughput, 0.2f, 0.95f);
            if (rng->NextFloat() > survival_probability) {
                break;
            }
            throughput /= survival_probability;
        }

        ray.origin = hit.position + hit.normal * 0.001f;
        ray.direction = SampleCosineHemisphere(hit.normal, rng);
    }

    return std::min(radiance, kSampleRadianceClamp);
}

static Ray GenerateCameraRay(
    const Camera& camera,
    const CameraBasis& basis,
    int pixel_x,
    int pixel_y,
    const RenderConfig& config,
    Rng* rng)
{
    const float aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    const float fov_scale = tanf((camera.vertical_fov_degrees * kPi / 180.0f) * 0.5f);
    const float jitter_x = rng->NextFloat();
    const float jitter_y = rng->NextFloat();

    const float sensor_x =
        (((static_cast<float>(pixel_x) + jitter_x) / static_cast<float>(config.width)) * 2.0f - 1.0f) * aspect * fov_scale;
    const float sensor_y =
        (1.0f - ((static_cast<float>(pixel_y) + jitter_y) / static_cast<float>(config.height)) * 2.0f) * fov_scale;

    Ray ray;
    ray.origin = camera.eye;
    ray.direction = Normalize(basis.forward + basis.right * sensor_x + basis.up * sensor_y);
    return ray;
}

static unsigned char ToneMapToByte(float luminance, float exposure)
{
    const float mapped = 1.0f - expf(-luminance * exposure);
    const float gamma = powf(Clamp(mapped, 0.0f, 1.0f), 1.0f / 2.2f);
    return static_cast<unsigned char>(Clamp(gamma * 255.0f, 0.0f, 255.0f));
}

static bool WritePgm(const char* output_path, const std::vector<unsigned char>& pixels, int width, int height)
{
    FILE* file = fopen(output_path, "wb");
    if (!file) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return false;
    }

    fprintf(file, "P5\n%d %d\n255\n", width, height);
    fwrite(&pixels[0], sizeof(unsigned char), pixels.size(), file);
    fclose(file);
    return true;
}

static bool WritePng(const char* output_path, const std::vector<unsigned char>& pixels, int width, int height)
{
    const int stride = width;
    return stbi_write_png(output_path, width, height, 1, &pixels[0], stride) != 0;
}

static bool WriteImageByExtension(
    const char* output_path,
    const std::vector<unsigned char>& pixels,
    int width,
    int height)
{
    const std::string extension = ExtensionOf(output_path);
    if (extension == ".png") {
        if (!WritePng(output_path, pixels, width, height)) {
            fprintf(stderr, "Failed to write PNG: %s\n", output_path);
            return false;
        }
        return true;
    }

    if (extension == ".pgm" || extension.empty()) {
        return WritePgm(output_path, pixels, width, height);
    }

    fprintf(stderr, "Unsupported output extension: %s\n", output_path);
    return false;
}

}  // namespace

bool RenderSceneToImage(const Scene& scene, const RenderConfig& config, const char* output_path)
{
    if (!output_path || config.width <= 0 || config.height <= 0) {
        return false;
    }

    const CameraBasis basis = BuildCameraBasis(scene.camera);
    const CameraLightState camera_light = BuildCameraLightState(scene, basis);
    std::vector<unsigned char> pixels(static_cast<size_t>(config.width * config.height));

    printf(
        "Rendering %dx%d, spp=%d, bounces=%d, emissive=%d, camera_spot=%s\n",
        config.width,
        config.height,
        config.samples_per_pixel,
        config.max_bounces,
        static_cast<int>(scene.emissive_triangles.size()),
        camera_light.enabled ? "on" : "off");

    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const uint32_t pixel_seed =
                config.seed ^ (static_cast<uint32_t>(x) * 1973u) ^ (static_cast<uint32_t>(y) * 9277u);
            Rng rng(pixel_seed);

            float accumulated = 0.0f;
            for (int sample_index = 0; sample_index < config.samples_per_pixel; ++sample_index) {
                const Ray ray = GenerateCameraRay(scene.camera, basis, x, y, config, &rng);
                accumulated += TracePath(scene, camera_light, config, ray, &rng);
            }

            const float average = accumulated / static_cast<float>(config.samples_per_pixel);
            pixels[static_cast<size_t>(x + y * config.width)] = ToneMapToByte(average, config.exposure);
        }

        if ((y % 16) == 0 || y + 1 == config.height) {
            printf("  line %d / %d\n", y + 1, config.height);
        }
    }

    return WriteImageByExtension(output_path, pixels, config.width, config.height);
}

}  // namespace liminal
