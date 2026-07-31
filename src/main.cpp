#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_state.h"
#include "llm_runtime.h"
#include "renderer.h"
#include "scene_compiler.h"
#include "turn_contract.h"
#include "turn_runner.h"

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
    printf("  --run-turn               Run one real headless turn through Ministral and render the resulting place\n");
    printf("  --model <path>           GGUF model path for --run-turn\n");
    printf("  --llm-temperature <f>    Sampling temperature for --run-turn\n");
    printf("  --llm-predict <n>        Maximum generated tokens for --run-turn\n");
    printf("  --use-json-grammar       Enable llama.cpp JSON grammar for --run-turn\n");
    printf("  --no-json-grammar        Disable llama.cpp JSON grammar for --run-turn\n");
    printf("  --prefer-candidate-scene Render a valid candidate_scene_text instead of the deterministic compiled scene\n");
    printf("  --dump-raw-turn          Print the raw structured model response after --run-turn\n");
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
    bool run_turn = false;
    bool prefer_candidate_scene = false;
    bool dump_raw_turn = false;
    liminal::LocationId selected_location = liminal::kLocationGate;
    liminal::HeadlessTurnConfig headless_turn_config;

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
        if (strcmp(argv[index], "--run-turn") == 0) {
            run_turn = true;
            continue;
        }
        if (strcmp(argv[index], "--model") == 0 && index + 1 < argc) {
            headless_turn_config.generation_config.model_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--llm-temperature") == 0 && index + 1 < argc) {
            if (!ReadFloat(argv[++index], &headless_turn_config.generation_config.temperature)) {
                fprintf(stderr, "Invalid LLM temperature value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--llm-predict") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &headless_turn_config.generation_config.n_predict)) {
                fprintf(stderr, "Invalid LLM predict value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--use-json-grammar") == 0) {
            headless_turn_config.generation_config.use_json_grammar = true;
            continue;
        }
        if (strcmp(argv[index], "--no-json-grammar") == 0) {
            headless_turn_config.generation_config.use_json_grammar = false;
            continue;
        }
        if (strcmp(argv[index], "--prefer-candidate-scene") == 0) {
            prefer_candidate_scene = true;
            continue;
        }
        if (strcmp(argv[index], "--dump-raw-turn") == 0) {
            dump_raw_turn = true;
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
    } else if (run_turn) {
        headless_turn_config.prefer_candidate_scene = prefer_candidate_scene;
        liminal::HeadlessTurnResult turn_result;
        if (!liminal::RunHeadlessTurn(selected_location, player_command, headless_turn_config, &turn_result, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Headless turn failed.");
            return 1;
        }

        scene = turn_result.rendered_scene;

        printf("Headless turn completed.\n");
        printf("Prompt tokens: %d\n", turn_result.prompt_tokens);
        printf("Generated tokens: %d\n", turn_result.generated_tokens);
        printf("Inference time: %.2f ms\n", turn_result.inference_time_ms);
        printf("Intent: %s\n", turn_result.turn_result.intent.empty() ? "(empty)" : turn_result.turn_result.intent.c_str());
        printf("Narration: %s\n", turn_result.turn_result.narration.empty() ? "(empty)" : turn_result.turn_result.narration.c_str());
        if (!turn_result.turn_result.clarification.empty()) {
            printf("Clarification: %s\n", turn_result.turn_result.clarification.c_str());
        }
        printf("Initial location: %s\n", liminal::LocationIdToString(turn_result.initial_spatial_state.location_id));
        printf("Updated location: %s\n", liminal::LocationIdToString(turn_result.updated_spatial_state.location_id));
        printf(
            "Candidate scene audit: %s\n",
            turn_result.turn_result.candidate_scene_included
                ? (turn_result.candidate_scene_valid ? "valid" : "invalid")
                : "not provided");
        if (turn_result.turn_result.candidate_scene_included && turn_result.candidate_scene_valid) {
            printf(
                "Candidate scene stats: %zu triangles, %zu materials\n",
                turn_result.candidate_scene.triangles.size(),
                turn_result.candidate_scene.materials.size());
        } else if (turn_result.turn_result.candidate_scene_included && !turn_result.candidate_scene_error.empty()) {
            printf("Candidate scene error: %s\n", turn_result.candidate_scene_error.c_str());
        }
        printf(
            "Rendered scene source: %s\n",
            turn_result.used_candidate_scene_for_render ? "candidate_scene_text" : "compiled_spatial_state");
        if (dump_raw_turn) {
            printf("\n=== Raw Turn Response ===\n%s\n", turn_result.raw_response_text.c_str());
            if (!turn_result.raw_scene_audit_response_text.empty()) {
                printf("\n=== Raw Scene Audit Response ===\n%s\n", turn_result.raw_scene_audit_response_text.c_str());
            }
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
        "%s: %.2f ms\n",
        run_turn ? "Preparation time" : "Load time",
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
