#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_state.h"
#include "llm_runtime.h"
#include "renderer.h"
#include "scene_compiler.h"
#include "turn_contract.h"

namespace {

static void PrintUsage()
{
    printf("Usage:\n");
    printf("  liminal_cornell_renderer [options]\n\n");
    printf("Options:\n");
    printf("  --scene <path>           Scene file to render (.scene or .obj)\n");
    printf("  --obj <path>             Legacy alias for --scene\n");
    printf("  --output <path>          Output image path (.png or .pgm)\n");
    printf("  --width <n>              Output width\n");
    printf("  --height <n>             Output height\n");
    printf("  --samples <n>            Samples per pixel\n");
    printf("  --bounces <n>            Maximum diffuse bounces\n");
    printf("  --direct-samples <n>     Direct light samples per hit\n");
    printf("  --seed <n>               Random seed\n");
    printf("  --exposure <f>           Tone mapping exposure\n");
    printf("  --location <id>          Canonical location id (gate, server_aisles, roof_watch)\n");
    printf("  --compile-location <id>  Compile a canonical spatial state and render it\n");
    printf("  --audit-scene-text <path> Load a .scene as text in memory, validate it, then render it\n");
    printf("  --dump-turn-contract     Print the structured turn prompt and exit\n");
    printf("  --dump-scene-audit-prompt Print the direct .scene audit prompt and exit\n");
    printf("  --llama-info             Print llama.cpp runtime information and exit\n");
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

static bool ReadFileText(const char* path, std::string* text)
{
    if (!path || !text) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    text->clear();
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        text->append(buffer);
    }
    fclose(file);
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    const char* scene_path = "assets/scenes/liminal_service_corridor.scene";
    const char* output_path = "output/liminal_service_corridor.png";
    const char* player_command = "look around";
    liminal::RenderConfig config;
    bool print_llama_info_only = false;
    bool dump_turn_contract = false;
    bool dump_scene_audit_prompt = false;
    bool compile_canonical_location = false;
    const char* audit_scene_text_path = 0;
    liminal::LocationId selected_location = liminal::kLocationGate;

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
        if (strcmp(argv[index], "--location") == 0 && index + 1 < argc) {
            if (!liminal::ParseLocationId(argv[++index], &selected_location)) {
                fprintf(stderr, "Invalid location value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--compile-location") == 0 && index + 1 < argc) {
            if (!liminal::ParseLocationId(argv[++index], &selected_location)) {
                fprintf(stderr, "Invalid compile location value.\n");
                return 1;
            }
            compile_canonical_location = true;
            continue;
        }
        if (strcmp(argv[index], "--audit-scene-text") == 0 && index + 1 < argc) {
            audit_scene_text_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--dump-turn-contract") == 0) {
            dump_turn_contract = true;
            continue;
        }
        if (strcmp(argv[index], "--dump-scene-audit-prompt") == 0) {
            dump_scene_audit_prompt = true;
            continue;
        }
        if (strcmp(argv[index], "--command") == 0 && index + 1 < argc) {
            player_command = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--llama-info") == 0) {
            print_llama_info_only = true;
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

    if (print_llama_info_only) {
        liminal::PrintLlmRuntimeInfo(stdout);
        liminal::ShutdownLlmRuntime();
        return 0;
    }

    if (dump_turn_contract || dump_scene_audit_prompt) {
        liminal::HardState hard_state = liminal::MakeInitialHardState();
        liminal::SoftState soft_state = liminal::MakeInitialSoftState();
        liminal::SpatialState spatial_state;
        if (!liminal::BuildCanonicalSpatialState(selected_location, &spatial_state)) {
            fprintf(stderr, "Cannot build canonical spatial state for location.\n");
            return 1;
        }
        hard_state.current_location_id = selected_location;
        hard_state.alert_level = spatial_state.alert_level;

        if (dump_turn_contract) {
            printf("%s\n", liminal::BuildTurnPrompt(hard_state, soft_state, spatial_state, player_command, true).c_str());
        } else {
            printf("%s\n", liminal::BuildSceneAuditPrompt(spatial_state).c_str());
        }
        return 0;
    }

    liminal::Scene scene;
    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    const std::chrono::steady_clock::time_point load_start = std::chrono::steady_clock::now();
    if (audit_scene_text_path) {
        std::string scene_text;
        if (!ReadFileText(audit_scene_text_path, &scene_text)) {
            fprintf(stderr, "Cannot read scene text file: %s\n", audit_scene_text_path);
            return 1;
        }
        if (!liminal::AuditSceneCandidateText(audit_scene_text_path, scene_text.c_str(), &scene, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Scene text audit failed.");
            return 1;
        }
    } else if (compile_canonical_location) {
        liminal::SpatialState spatial_state;
        if (!liminal::BuildCanonicalSpatialState(selected_location, &spatial_state) ||
            !liminal::CompileSpatialStateToScene(spatial_state, &scene, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Scene compilation failed.");
            return 1;
        }
    } else {
        if (!liminal::LoadSceneFromPath(scene_path, &scene, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Scene loading failed.");
            return 1;
        }
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
    if (!liminal::RenderSceneToImage(scene, config, output_path)) {
        fprintf(stderr, "Rendering failed.\n");
        return 1;
    }
    const std::chrono::steady_clock::time_point render_end = std::chrono::steady_clock::now();

    printf(
        "Render time: %.2f ms\n",
        std::chrono::duration<double, std::milli>(render_end - render_start).count());
    printf("Wrote %s\n", output_path);
    liminal::ShutdownLlmRuntime();
    return 0;
}
