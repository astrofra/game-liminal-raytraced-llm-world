#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "renderer.h"

namespace {

static void PrintUsage()
{
    printf("Usage:\n");
    printf("  liminal_cornell_renderer [options]\n\n");
    printf("Options:\n");
    printf("  --scene <path>           Scene file to render (.scene or .obj)\n");
    printf("  --obj <path>             Legacy alias for --scene\n");
    printf("  --output <path>          Output PGM path\n");
    printf("  --width <n>              Output width\n");
    printf("  --height <n>             Output height\n");
    printf("  --samples <n>            Samples per pixel\n");
    printf("  --bounces <n>            Maximum diffuse bounces\n");
    printf("  --direct-samples <n>     Direct light samples per hit\n");
    printf("  --seed <n>               Random seed\n");
    printf("  --exposure <f>           Tone mapping exposure\n");
}

static bool ReadInt(const char* text, int* value)
{
    if (!text || !value) {
        return false;
    }

    char* end = 0;
    const long parsed = strtol(text, &end, 10);
    if (end == text) {
        return false;
    }

    *value = static_cast<int>(parsed);
    return true;
}

static bool ReadUnsigned(const char* text, unsigned int* value)
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

static bool ReadFloat(const char* text, float* value)
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

}  // namespace

int main(int argc, char** argv)
{
    const char* scene_path = "assets/scenes/liminal_service_corridor.scene";
    const char* output_path = "output/liminal_service_corridor.pgm";
    liminal::RenderConfig config;

    for (int index = 1; index < argc; ++index) {
        if ((strcmp(argv[index], "--scene") == 0 || strcmp(argv[index], "--obj") == 0) && index + 1 < argc) {
            scene_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--output") == 0 && index + 1 < argc) {
            output_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--width") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.width)) {
                fprintf(stderr, "Invalid width value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--height") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.height)) {
                fprintf(stderr, "Invalid height value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--samples") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.samples_per_pixel)) {
                fprintf(stderr, "Invalid samples value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--bounces") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.max_bounces)) {
                fprintf(stderr, "Invalid bounces value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--direct-samples") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.direct_light_samples)) {
                fprintf(stderr, "Invalid direct light samples value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--seed") == 0 && index + 1 < argc) {
            unsigned int seed = 0;
            if (!ReadUnsigned(argv[++index], &seed)) {
                fprintf(stderr, "Invalid seed value.\n");
                return 1;
            }
            config.seed = seed;
            continue;
        }
        if (strcmp(argv[index], "--exposure") == 0 && index + 1 < argc) {
            if (!ReadFloat(argv[++index], &config.exposure)) {
                fprintf(stderr, "Invalid exposure value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            PrintUsage();
            return 0;
        }

        fprintf(stderr, "Unknown option: %s\n", argv[index]);
        PrintUsage();
        return 1;
    }

    liminal::Scene scene;
    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    const std::chrono::steady_clock::time_point load_start = std::chrono::steady_clock::now();
    if (!liminal::LoadSceneFromPath(scene_path, &scene, error_buffer, sizeof(error_buffer))) {
        fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Scene loading failed.");
        return 1;
    }
    const std::chrono::steady_clock::time_point load_end = std::chrono::steady_clock::now();

    printf(
        "Loaded scene %s\n",
        scene.name.empty() ? "(unnamed)" : scene.name.c_str());
    printf(
        "Loaded %zu triangles, %zu materials, %zu emissive triangles\n",
        scene.triangles.size(),
        scene.materials.size(),
        scene.emissive_triangles.size());
    printf(
        "Load time: %.2f ms\n",
        std::chrono::duration<double, std::milli>(load_end - load_start).count());

    const std::chrono::steady_clock::time_point render_start = std::chrono::steady_clock::now();
    if (!liminal::RenderSceneToPgm(scene, config, output_path)) {
        fprintf(stderr, "Rendering failed.\n");
        return 1;
    }
    const std::chrono::steady_clock::time_point render_end = std::chrono::steady_clock::now();

    printf(
        "Render time: %.2f ms\n",
        std::chrono::duration<double, std::milli>(render_end - render_start).count());
    printf("Wrote %s\n", output_path);
    return 0;
}
