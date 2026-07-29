#include "renderer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace liminal {

namespace {

struct CameraBasis {
    Vec3 forward;
    Vec3 right;
    Vec3 up;
};

static const float kSampleRadianceClamp = 6.0f;

static CameraBasis BuildCameraBasis(const Camera& camera)
{
    CameraBasis basis;
    basis.forward = Normalize(camera.target - camera.eye);
    basis.right = Normalize(Cross(basis.forward, camera.up));
    basis.up = Normalize(Cross(basis.right, basis.forward));
    return basis;
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

static float EstimateDirectLighting(
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

static float TracePath(const Scene& scene, const RenderConfig& config, const Ray& camera_ray, Rng* rng)
{
    Ray ray = camera_ray;
    float throughput = 1.0f;
    float radiance = 0.0f;

    for (int bounce = 0; bounce < config.max_bounces; ++bounce) {
        Hit hit;
        if (!IntersectScene(scene, ray, kHuge, &hit)) {
            break;
        }

        const Triangle& triangle = scene.triangles[hit.triangle_index];
        const Material& material = scene.materials[triangle.material_index];

        if (material.emission > 0.0f) {
            radiance += throughput * material.emission;
            break;
        }

        radiance += throughput *
            EstimateDirectLighting(scene, hit, material, config.direct_light_samples, rng);

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

    // Keep the image gritty without letting a few rare paths dominate whole pixels.
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

}  // namespace

bool RenderSceneToPgm(const Scene& scene, const RenderConfig& config, const char* output_path)
{
    if (!output_path || config.width <= 0 || config.height <= 0) {
        return false;
    }

    FILE* file = fopen(output_path, "wb");
    if (!file) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return false;
    }

    fprintf(file, "P5\n%d %d\n255\n", config.width, config.height);

    const CameraBasis basis = BuildCameraBasis(scene.camera);
    std::vector<unsigned char> row(static_cast<size_t>(config.width));

    printf(
        "Rendering %dx%d, spp=%d, bounces=%d, lights=%d\n",
        config.width,
        config.height,
        config.samples_per_pixel,
        config.max_bounces,
        static_cast<int>(scene.emissive_triangles.size()));

    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const uint32_t pixel_seed =
                config.seed ^ (static_cast<uint32_t>(x) * 1973u) ^ (static_cast<uint32_t>(y) * 9277u);
            Rng rng(pixel_seed);

            float accumulated = 0.0f;
            for (int sample_index = 0; sample_index < config.samples_per_pixel; ++sample_index) {
                const Ray ray = GenerateCameraRay(scene.camera, basis, x, y, config, &rng);
                accumulated += TracePath(scene, config, ray, &rng);
            }

            const float average = accumulated / static_cast<float>(config.samples_per_pixel);
            row[static_cast<size_t>(x)] = ToneMapToByte(average, config.exposure);
        }

        fwrite(&row[0], sizeof(unsigned char), row.size(), file);

        if ((y % 16) == 0 || y + 1 == config.height) {
            printf("  line %d / %d\n", y + 1, config.height);
        }
    }

    fclose(file);
    return true;
}

}  // namespace liminal
