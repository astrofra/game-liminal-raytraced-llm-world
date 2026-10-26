#include "file_io.h"
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <random>
#include "nlohmann/json.hpp"

#include "animated_view.h"
#include "game_state.h"
#include "llm_runtime.h"
#include "renderer.h"
#include "sdl_frontend.h"
#include "scene_compiler.h"
#include "turn_contract.h"
#include "turn_runner.h"
#include "view_post_process.h"

namespace {

static void PrintUsage()
{
    printf("Usage:\n");
    printf("  liminal_cornell_renderer [options]\n\n");
    printf("Options:\n");
    printf("  --scene-generation-mode <hybrid|dynamic|local> Discover persistent places along open cardinal exits\n");
    printf("  --generate-dynamic-scene <brief.txt> Generate, audit and render a new scene\n");
    printf("  --audit-scene-view <file> Audit actual geometry and render; writes <scene-artifacts>.audit.json\n");
    printf("  --view-landmark <name>   Require a visible named solid in --audit-scene-view\n");
    printf("  --scene-method <two-pass|direct|hybrid|local> Benchmark method (default: two-pass)\n");
    printf("  --scene-entry <north|east|south|west> Arrival side in benchmark\n");
    printf("  --scene-artifacts <prefix> Write .design.json, .scene and .metrics.json\n");
    printf("  --scene-max-calls <1..3> Diagnostic composition limit (default: 3)\n");
    printf("  --scene-timeout-ms <n> Total composition safety deadline (default: 900000)\n");
    printf("  --scene <path>           Scene file to render (.scene or .obj)\n");
    printf("  --obj <path>             Legacy alias for --scene\n");
    printf("  --output <path>          Output image path (.png or .pgm)\n");
    printf("  --width <n>              Output width\n");
    printf("  --height <n>             Output height\n");
    printf("  --samples <n>            Samples per pixel\n");
    printf("  --bounces <n>            Maximum diffuse events per path\n");
    printf("  --glass-bounces <n>      Maximum dielectric events per path\n");
    printf("  --direct-samples <n>     Direct light samples per hit\n");
    printf("  --seed <n>               Random seed\n");
    printf("  --exposure <f>           Tone mapping exposure\n");
    printf("  --location <id>          Starting location (quarry_threshold); legacy diagnostic fixtures: gate, server_aisles, roof_watch\n");
    printf("  --compile-location <id>  Compile a canonical spatial state and render it\n");
    printf("  --audit-scene-text <path> Load a .scene as text in memory, validate it, then render it\n");
    printf("  --run-turn               Run one real headless turn through Gemma and render the resulting place\n");
    printf("  --run-session            Run a multi-turn headless session through Gemma and render the last place\n");
    printf("  --sdl                    Launch the SDL3 interactive frontend with streaming LLM output\n");
    printf("  --view-animation-fps <f> Animated viewport playback rate (default: 6)\n");
    printf("  --animated-view-fail-frame <n> Force animation image 1..7 to fail for diagnostics\n");
    printf("  --animated-view-debug    Log displayed image, direction, generation and worker state\n");
    printf("  --animated-view-self-test Validate camera endpoints and N=1/2/4/8 ping-pong sequences\n");
    printf("  --navigation-self-test Validate localized directions, the single starting place and generated exploration\n");
    printf("  --view-post-process-self-test Validate optical effects and post-optical RGB grain\n");
    printf("  --no-view-post-process  Disable peripheral optics and RGB grain in SDL\n");
    printf("  --view-blur-radius <n>  Peripheral blur radius in source pixels (default: 3)\n");
    printf("  --view-dispersion <f>   Maximum peripheral RGB shift in source pixels (default: 6)\n");
    printf("  --view-grain <f>        Post-optical RGB grain amplitude (default: 7)\n");
    printf("  --view-post-process-debug Log post-process cost and affected pixel count\n");
    printf("  --sdl-smoke-language <code> Diagnostic startup language: en/fr/no/da/de/it (default en)\n");
    printf("  --sdl-smoke-test-ms <n> Auto-select a language and close SDL cleanly after n milliseconds\n");
    printf("  --sdl-smoke-command <text> Inject text and Return during the timed SDL smoke test\n");
    printf("  --write-initial-state <json> Export canonical initial state without inference\n");
    printf("  --llm-profile <name>     gemma-4-e4b-q4\n");
    printf("  --llm-context <n>        Fixed context limit (default 8192)\n");
    printf("  --continuity-memory-tokens <n>  Recent memory cap (default 1024; output reserve unchanged)\n");
    printf("  --llm-gpu-layers <n>     Offloaded model layers; 0 isolates CPU qualification\n");
    printf("  --llm-timeout-ms <n>     Per-call deadline including model load\n");
    printf("  --llm-seed <n>           Sampling seed\n");
    printf("  --llm-probe <json>       Isolated diagnostic completion (role/content array)\n");
    printf("  --llm-probe-repeat <n>   Diagnostic repeated independent calls (1-100)\n");
    printf("  --llm-probe-schema <json> Diagnostic JSON Schema constraint\n");
    printf("  --llm-probe-output <json> Save probe channels and diagnostics\n");
    printf("  --llm-cancel-after <n>   Probe cancellation after n final chunks\n");
    printf("  --model <path>           GGUF model path for --run-turn\n");
    printf("  --llm-temperature <f>    Sampling temperature for --run-turn\n");
    printf("  --llm-predict <n>        Maximum generated tokens for --run-turn\n");
    printf("  --use-json-grammar       Enable llama.cpp JSON grammar for --run-turn\n");
    printf("  --no-json-grammar        Disable llama.cpp JSON grammar for --run-turn\n");
    printf("  --command <text>         Player command; may be repeated for --run-session\n");
    printf("  --command-file <path>    Read one player command per non-empty line\n");
    printf("  --load-state <path>      Load session state JSON before --run-turn or --run-session\n");
    printf("  --save-state <path>      Save resulting session state JSON after --run-turn or --run-session\n");
    printf("  --research-dir <path>    Diagnostic archive root override; SDL always creates a fresh run directory\n");
    printf("  --dump-session-state     Print the final session state summary\n");
    printf("  --dump-session-history   Print the final session transcript summary\n");
    printf("  --prefer-candidate-scene Render a valid candidate_scene_text instead of the deterministic compiled scene\n");
    printf("  --dump-raw-turn          Print the raw structured model response after --run-turn\n");
    printf("  --dump-turn-contract     Print the structured turn prompt and exit\n");
    printf("  --dump-generated-room-prompt Print the generated-room JSON prompt and exit\n");
    printf("  --generated-room-direction <dir> Direction used with --dump-generated-room-prompt (north, east, south, west)\n");
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

    FILE* file = liminal::OpenUtf8File(path, "rb");
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

static bool WriteFileText(const char* path, const std::string& text)
{
    if (!path) {
        return false;
    }

    FILE* file = liminal::OpenUtf8File(path, "wb");
    if (!file) {
        return false;
    }

    const size_t written = fwrite(text.data(), 1, text.size(), file);
    fclose(file);
    return written == text.size();
}

static std::string TrimWhitespaceCopy(const std::string& text)
{
    size_t start = 0;
    while (start < text.size()) {
        const char value = text[start];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        ++start;
    }

    size_t end = text.size();
    while (end > start) {
        const char value = text[end - 1];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        --end;
    }

    return text.substr(start, end - start);
}

static bool ReadCommandFile(const char* path, std::vector<std::string>* commands)
{
    if (!path || !commands) {
        return false;
    }

    std::string text;
    if (!ReadFileText(path, &text)) {
        return false;
    }

    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const size_t length = end == std::string::npos ? text.size() - start : end - start;
        std::string line = TrimWhitespaceCopy(text.substr(start, length));
        if (!line.empty() && line[0] != '#') {
            commands->push_back(line);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return true;
}

static bool LoadSessionStateFromPath(const char* path, liminal::SessionState* session_state, char* error_buffer, size_t error_buffer_size)
{
    std::string text;
    if (!ReadFileText(path, &text)) {
        snprintf(error_buffer, error_buffer_size, "Cannot read session state file: %s", path ? path : "(null)");
        return false;
    }
    return liminal::ParseSessionStateFromJson(text.c_str(), session_state, error_buffer, error_buffer_size);
}

static bool SaveSessionStateToPath(const char* path, const liminal::SessionState& session_state, char* error_buffer, size_t error_buffer_size)
{
    std::string json_text;
    if (!liminal::SerializeSessionStateToJsonString(session_state, &json_text)) {
        snprintf(error_buffer, error_buffer_size, "Cannot serialize session state.");
        return false;
    }
    if (!WriteFileText(path, json_text)) {
        snprintf(error_buffer, error_buffer_size, "Cannot write session state file: %s", path ? path : "(null)");
        return false;
    }
    return true;
}

static void PrintTurnSummary(const liminal::HeadlessTurnResult& turn_result, bool dump_raw_turn)
{
    liminal::SessionState initial_session_state;
    initial_session_state.current_place_id = turn_result.initial_place_id;
    initial_session_state.spatial_state = turn_result.initial_spatial_state;
    liminal::NormalizeSessionState(&initial_session_state);

    liminal::SessionState updated_session_state;
    updated_session_state.current_place_id = turn_result.updated_place_id;
    updated_session_state.spatial_state = turn_result.updated_spatial_state;
    updated_session_state.generated_rooms = turn_result.generated_rooms_to_add;
    liminal::NormalizeSessionState(&updated_session_state);

    const char* rendered_scene_source =
        turn_result.used_candidate_scene_for_render
            ? "candidate_scene_text"
            : (liminal::IsGeneratedPlaceId(turn_result.updated_place_id) ? "generated_room_cache" : "compiled_spatial_state");
    const char* generated_room_metadata_source =
        turn_result.generated_room_metadata_fallback_used ? "fallback" : "llm";
    const char* generated_room_scene_source =
        turn_result.generated_room_scene_source.empty()
            ? (turn_result.generated_room_scene_fallback_used ? "fallback" : "unknown")
            : turn_result.generated_room_scene_source.c_str();

    printf("Turn %d completed.\n", turn_result.initial_hard_state.turn_number);
    printf("Prompt tokens: %d\n", turn_result.prompt_tokens);
    printf("Generated tokens: %d\n", turn_result.generated_tokens);
    printf("Inference time: %.2f ms\n", turn_result.inference_time_ms);
    printf("Turn JSON repair: %s\n", turn_result.used_turn_repair ? "applied" : "not needed");
    printf("Turn fallback: %s\n", turn_result.used_turn_fallback ? "applied" : "not needed");
    printf("Intent: %s\n", turn_result.turn_result.intent.empty() ? "(empty)" : turn_result.turn_result.intent.c_str());
    printf("Narration: %s\n", turn_result.turn_result.narration.empty() ? "(empty)" : turn_result.turn_result.narration.c_str());
    if (!turn_result.turn_result.clarification.empty()) {
        printf("Clarification: %s\n", turn_result.turn_result.clarification.c_str());
    }
    printf("Initial location: %s\n", liminal::LocationIdToString(turn_result.initial_spatial_state.location_id));
    printf("Updated location: %s\n", liminal::LocationIdToString(turn_result.updated_spatial_state.location_id));
    printf("Initial place: %s\n", liminal::DescribeCurrentPlaceLabel(initial_session_state).c_str());
    printf("Updated place: %s\n", liminal::DescribeCurrentPlaceLabel(updated_session_state).c_str());
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
        rendered_scene_source);
    if (turn_result.generated_room_created) {
        printf("Generated room metadata source: %s\n", generated_room_metadata_source);
        printf("Generated room scene source: %s\n", generated_room_scene_source);
    } else if (turn_result.generated_room_cache_refreshed) {
        printf("Generated room cache refresh source: %s\n", generated_room_scene_source);
    }
    if (dump_raw_turn) {
        printf("\n=== Raw Turn Response ===\n%s\n", turn_result.raw_response_text.c_str());
        if (!turn_result.repair_response_text.empty()) {
            printf("\n=== Repair Turn Response ===\n%s\n", turn_result.repair_response_text.c_str());
        }
        if (!turn_result.raw_scene_audit_response_text.empty()) {
            printf("\n=== Raw Scene Audit Response ===\n%s\n", turn_result.raw_scene_audit_response_text.c_str());
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    struct RuntimeShutdown { ~RuntimeShutdown() { liminal::ShutdownLlmRuntime(); } } runtime_shutdown;
#if defined(_WIN32)
    int wide_count = 0;
    LPWSTR* wide_args = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (!wide_args) return 1;
    std::vector<std::string> utf8_args;
    utf8_args.reserve(wide_count);
    for (int index = 0; index < wide_count; ++index) {
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_args[index], -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) { LocalFree(wide_args); return 1; }
        std::string text(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_args[index], -1, &text[0], bytes, nullptr, nullptr);
        text.pop_back();
        utf8_args.push_back(text);
    }
    LocalFree(wide_args);
    std::vector<char*> utf8_pointers;
    for (auto& argument : utf8_args) utf8_pointers.push_back(&argument[0]);
    utf8_pointers.push_back(nullptr);
    argc = wide_count;
    argv = utf8_pointers.data();
#endif
    const char* scene_path = "assets/scenes/liminal_service_corridor.scene";
    const char* output_path = "output/liminal_service_corridor.png";
    const char* player_command = "look around";
    liminal::RenderConfig config;
    bool print_llama_info_only = false;
    const char* initial_state_output = nullptr;
    const char* llm_probe_path = nullptr;
    const char* llm_probe_output = nullptr;
    const char* llm_probe_schema = nullptr;
    int llm_probe_repeat = 1;
    int llm_cancel_after = -1;
    int llm_cancel_after_checks = -1;
    bool llm_cancel_during_load = false;
    bool dump_turn_contract = false;
    bool dump_generated_room_prompt = false;
    bool dump_scene_audit_prompt = false;
    bool compile_canonical_location = false;
    const char* audit_scene_text_path = 0;
    const char* audit_scene_view_path = nullptr;
    liminal::SceneViewConstraints scene_view;
    bool run_turn = false;
    bool run_session = false;
    bool run_sdl = argc==1;
    bool run_animated_view_self_test = false;
    bool run_navigation_self_test = false;
    bool run_view_post_process_self_test = false;
    float view_animation_fps = 6.0f;
    int animated_view_fail_frame = -1;
    bool animated_view_debug = false;
    liminal::PeripheralPostProcessConfig view_post_process_config;
    liminal::GameLanguage sdl_smoke_test_language = liminal::kGameLanguageEnglish;
    bool sdl_smoke_language_requested = false;
    int sdl_smoke_test_duration_ms = 0;
    const char* sdl_smoke_test_command = 0;
    bool prefer_candidate_scene = false;
    bool dump_raw_turn = false;
    bool dump_session_state = false;
    bool dump_session_history = false;
    const char* command_file_path = 0;
    const char* load_state_path = 0;
    const char* save_state_path = 0;
    const char* research_root = nullptr;
    bool llm_seed_explicit = false;
    liminal::LocationId selected_location = liminal::kLocationQuarryThreshold;
    liminal::CardinalDirection generated_room_direction = liminal::kDirectionNorth;
    liminal::HeadlessTurnConfig headless_turn_config;
    headless_turn_config.local_scenes=true;
    const char* dynamic_brief_path = nullptr;
    std::string scene_artifacts = "dynamic-scene";
    bool scene_artifacts_requested = false;
    liminal::SceneDesignContext dynamic_context;
    bool benchmark_hybrid=false;
    std::vector<std::string> session_commands;

    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--audit-scene-view") == 0 && index+1<argc) {
            audit_scene_view_path=argv[++index];continue;
        }
        if (strcmp(argv[index], "--view-landmark") == 0 && index+1<argc) {
            scene_view.landmark=argv[++index];continue;
        }
        if (strcmp(argv[index], "--scene-generation-mode") == 0 && index+1<argc) {
            const std::string mode=argv[++index];
            if(mode!="dynamic" && mode!="hybrid" && mode!="local")return 1;
            headless_turn_config.dynamic_scenes=mode=="dynamic";
            headless_turn_config.local_scenes=mode=="local";continue;
        }
        if (strcmp(argv[index], "--generate-dynamic-scene") == 0 && index+1<argc) {
            dynamic_brief_path=argv[++index];continue;
        }
        if (strcmp(argv[index], "--scene-method") == 0 && index+1<argc) {
            const std::string mode=argv[++index];
            if(mode!="two-pass" && mode!="direct" && mode!="hybrid" && mode!="local")return 1;
            benchmark_hybrid=mode=="hybrid";
            headless_turn_config.dynamic_scene_config.direct=mode=="direct"||mode=="local";
            headless_turn_config.dynamic_scene_config.fixed_frame=mode=="local";continue;
        }
        if (strcmp(argv[index], "--scene-entry") == 0 && index+1<argc) {
            dynamic_context.entry_direction=argv[++index];
            liminal::CardinalDirection dir;
            if(!liminal::ParseCardinalDirection(dynamic_context.entry_direction.c_str(),&dir))return 1;
            continue;
        }
        if (strcmp(argv[index], "--scene-artifacts") == 0 && index+1<argc) {
            scene_artifacts=argv[++index];scene_artifacts_requested=true;continue;
        }
        if (strcmp(argv[index], "--scene-max-calls") == 0 && index+1<argc) {
            const int calls=atoi(argv[++index]);if(calls<1||calls>3)return 1;
            headless_turn_config.dynamic_scene_config.max_repairs=calls-1;continue;
        }
        if (strcmp(argv[index], "--scene-timeout-ms") == 0 && index+1<argc) {
            if(!ReadInt(argv[++index],&headless_turn_config.dynamic_scene_config.total_timeout_ms) ||
               headless_turn_config.dynamic_scene_config.total_timeout_ms<=0)return 1;
            continue;
        }
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
        if (strcmp(argv[index], "--glass-bounces") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &config.max_glass_bounces)) {
                fprintf(stderr, "Invalid glass bounces value.\n");
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
        if (strcmp(argv[index], "--run-session") == 0) {
            run_session = true;
            continue;
        }
        if (strcmp(argv[index], "--sdl") == 0) {
            run_sdl = true;
            continue;
        }
        if (strcmp(argv[index], "--view-animation-fps") == 0 && index + 1 < argc) {
            if (!ReadFloat(argv[++index], &view_animation_fps) || view_animation_fps <= 0.0f) {
                fprintf(stderr, "Invalid view animation FPS value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--animated-view-fail-frame") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &animated_view_fail_frame) ||
                animated_view_fail_frame < 1 || animated_view_fail_frame >= liminal::kAnimatedViewImageCapacity) {
                fprintf(stderr, "Animated view failure frame must be between 1 and 7.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--animated-view-self-test") == 0) {
            run_animated_view_self_test = true;
            continue;
        }
        if (strcmp(argv[index], "--navigation-self-test") == 0) {
            run_navigation_self_test = true;
            continue;
        }
        if (strcmp(argv[index], "--animated-view-debug") == 0) {
            animated_view_debug = true;
            continue;
        }
        if (strcmp(argv[index], "--view-post-process-self-test") == 0) {
            run_view_post_process_self_test = true;
            continue;
        }
        if (strcmp(argv[index], "--no-view-post-process") == 0) {
            view_post_process_config.enabled = false;
            continue;
        }
        if (strcmp(argv[index], "--view-blur-radius") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &view_post_process_config.blur_radius_pixels) ||
                view_post_process_config.blur_radius_pixels < 0 || view_post_process_config.blur_radius_pixels > 12) {
                fprintf(stderr, "View blur radius must be between 0 and 12.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--view-dispersion") == 0 && index + 1 < argc) {
            if (!ReadFloat(argv[++index], &view_post_process_config.dispersion_pixels) ||
                view_post_process_config.dispersion_pixels < 0.0f || view_post_process_config.dispersion_pixels > 16.0f) {
                fprintf(stderr, "View dispersion must be between 0 and 16 source pixels.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--view-grain") == 0 && index + 1 < argc) {
            if (!ReadFloat(argv[++index], &view_post_process_config.grain_strength) ||
                view_post_process_config.grain_strength < 0.0f || view_post_process_config.grain_strength > 32.0f) {
                fprintf(stderr, "View grain amplitude must be between 0 and 32.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--view-post-process-debug") == 0) {
            view_post_process_config.debug_logging = true;
            continue;
        }
        if (strcmp(argv[index], "--sdl-smoke-test-ms") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &sdl_smoke_test_duration_ms) || sdl_smoke_test_duration_ms <= 0) {
                fprintf(stderr, "SDL smoke-test duration must be positive.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--sdl-smoke-language") == 0 && index + 1 < argc) {
            sdl_smoke_language_requested = true;
            if (!liminal::ParseGameLanguage(argv[++index], &sdl_smoke_test_language)) {
                fprintf(stderr, "Unknown SDL smoke-test language.\n"); return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--sdl-smoke-command") == 0 && index + 1 < argc) {
            sdl_smoke_test_command = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--llm-profile") == 0 && index + 1 < argc) {
            std::string error;
            if (!liminal::ConfigureLlmProfile(argv[++index], &headless_turn_config.generation_config, &error)) {
                fprintf(stderr, "%s\n", error.c_str()); return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--write-initial-state") == 0 && index + 1 < argc) { initial_state_output = argv[++index]; continue; }
        if (strcmp(argv[index], "--llm-cancel-during-load") == 0) { llm_cancel_during_load = true; continue; }
        if (strcmp(argv[index], "--llm-probe") == 0 && index + 1 < argc) { llm_probe_path = argv[++index]; continue; }
        if (strcmp(argv[index], "--llm-probe-repeat") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &llm_probe_repeat) || llm_probe_repeat < 1 || llm_probe_repeat > 100) return 1;
            continue;
        }
        if (strcmp(argv[index], "--llm-probe-schema") == 0 && index + 1 < argc) { llm_probe_schema=argv[++index]; continue; }
        if (strcmp(argv[index], "--llm-probe-output") == 0 && index + 1 < argc) { llm_probe_output = argv[++index]; continue; }
        if (strcmp(argv[index], "--llm-cancel-after") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &llm_cancel_after)) return 1; continue;
        }
        if (strcmp(argv[index], "--llm-cancel-after-checks") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &llm_cancel_after_checks)) return 1; continue;
        }
        if (strcmp(argv[index], "--llm-seed") == 0 && index + 1 < argc) {
            int seed = 0; if (!ReadInt(argv[++index], &seed) || seed < 0) return 1;
            llm_seed_explicit = true;
            headless_turn_config.generation_config.seed = static_cast<unsigned int>(seed); continue;
        }
        if (strcmp(argv[index], "--llm-context") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &headless_turn_config.generation_config.n_ctx)) return 1; continue;
        }
        if (strcmp(argv[index], "--continuity-memory-tokens") == 0 && index + 1 < argc) {
            int& cap=headless_turn_config.generation_config.continuity_memory_tokens;
            if(!ReadInt(argv[++index],&cap)||cap<0||cap>32768)return 1;
            continue;
        }
        if (strcmp(argv[index], "--llm-timeout-ms") == 0 && index + 1 < argc) {
            if (!ReadInt(argv[++index], &headless_turn_config.generation_config.timeout_ms)) return 1; continue;
        }
        if (strcmp(argv[index], "--llm-gpu-layers") == 0 && index + 1 < argc) {
            if(!ReadInt(argv[++index],&headless_turn_config.generation_config.n_gpu_layers) ||
               headless_turn_config.generation_config.n_gpu_layers<0 || headless_turn_config.generation_config.n_gpu_layers>999)return 1;
            continue;
        }
        if (strcmp(argv[index], "--model") == 0 && index + 1 < argc) {
            headless_turn_config.generation_config.model_path = argv[++index];
            headless_turn_config.generation_config.profile_name = "custom";
            headless_turn_config.generation_config.model_sha256.clear();
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
        if (strcmp(argv[index], "--dump-session-state") == 0) {
            dump_session_state = true;
            continue;
        }
        if (strcmp(argv[index], "--dump-session-history") == 0) {
            dump_session_history = true;
            continue;
        }
        if (strcmp(argv[index], "--dump-turn-contract") == 0) {
            dump_turn_contract = true;
            continue;
        }
        if (strcmp(argv[index], "--dump-generated-room-prompt") == 0) {
            dump_generated_room_prompt = true;
            continue;
        }
        if (strcmp(argv[index], "--generated-room-direction") == 0 && index + 1 < argc) {
            if (!liminal::ParseCardinalDirection(argv[++index], &generated_room_direction) ||
                generated_room_direction == liminal::kDirectionUnknown) {
                fprintf(stderr, "Invalid generated room direction value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--dump-scene-audit-prompt") == 0) {
            dump_scene_audit_prompt = true;
            continue;
        }
        if (strcmp(argv[index], "--command") == 0 && index + 1 < argc) {
            player_command = argv[++index];
            session_commands.push_back(player_command);
            continue;
        }
        if (strcmp(argv[index], "--command-file") == 0 && index + 1 < argc) {
            command_file_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--load-state") == 0 && index + 1 < argc) {
            load_state_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--save-state") == 0 && index + 1 < argc) {
            save_state_path = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--research-dir") == 0 && index + 1 < argc) {
            research_root = argv[++index];continue;
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

    liminal::Scene scene;
    liminal::SessionState pending_saved_session;
    bool have_pending_save=false;
    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    if ((run_turn && run_session) || (run_sdl && (run_turn || run_session))) {
        fprintf(stderr, "Choose either --run-turn, --run-session or --sdl.\n");
        return 1;
    }
    if ((sdl_smoke_test_command || sdl_smoke_language_requested) && sdl_smoke_test_duration_ms <= 0) {
        fprintf(stderr, "--sdl-smoke-command/language require --sdl-smoke-test-ms.\n");
        return 1;
    }

    if (command_file_path && !ReadCommandFile(command_file_path, &session_commands)) {
        fprintf(stderr, "Cannot read command file: %s\n", command_file_path);
        return 1;
    }

    if (run_turn && session_commands.size() > 1) {
        fprintf(stderr, "--run-turn accepts only one command. Use --run-session for multiple commands.\n");
        return 1;
    }

    if (run_turn && !session_commands.empty()) {
        player_command = session_commands.back().c_str();
    }

    if (dynamic_brief_path) {
        if(!ReadFileText(dynamic_brief_path,&dynamic_context.brief))return 1;
        liminal::DynamicSceneResult generated;
        struct BenchProgress { int phase=-1; std::chrono::steady_clock::time_point last{}; } bench_progress;
        auto progress=[](liminal::DynamicScenePhase phase,const char* text,const char*,void* opaque) {
            auto& state=*static_cast<BenchProgress*>(opaque);const auto now=std::chrono::steady_clock::now();
            if(state.phase!=static_cast<int>(phase) || now-state.last>std::chrono::seconds(30)) {
                const char* labels[]={"design","program","repair","audit","preview"};
                fprintf(stderr,"[dynamic-scene] phase=%s final_bytes=%zu\n",labels[static_cast<int>(phase)],text?strlen(text):0);
                fflush(stderr);state.last=now;state.phase=static_cast<int>(phase);
            }
            return true;
        };
        bool ok=false;
        if(benchmark_hybrid) {
            const auto started=std::chrono::steady_clock::now();
            liminal::SessionState state;
            state.current_place_id="generated:room_900000";state.origin_place_id=state.current_place_id;
            state.next_generated_room_index=900001;state.hard_state.current_location_id=liminal::kLocationUnknown;
            state.spatial_state.room_title="Benchmark origin";state.spatial_state.room_summary=dynamic_context.brief;
            state.spatial_state.time_of_day=liminal::kTimeDay;state.spatial_state.visibility_level=liminal::kVisibilityClear;
            state.spatial_state.desert_state=liminal::kDesertStill;state.spatial_state.interior_density=liminal::kInteriorSparse;
            liminal::GeneratedRoom origin;origin.room_id=state.current_place_id;origin.spatial_state=state.spatial_state;
            state.generated_rooms.push_back(origin);
            liminal::CardinalDirection arrival;liminal::ParseCardinalDirection(dynamic_context.entry_direction.c_str(),&arrival);
            liminal::HeadlessTurnResult turn;
            auto hybrid_config=headless_turn_config;hybrid_config.dynamic_scenes=false;
            ok=liminal::RunHeadlessTurnFromState(state,liminal::CardinalDirectionToString(liminal::OppositeCardinalDirection(arrival)),
                hybrid_config,&turn,error_buffer,sizeof(error_buffer));
            generated.scene=turn.rendered_scene;generated.scene_text=turn.rendered_scene_text;
            if(!ok)generated.error=error_buffer;
            generated.metrics_json=nlohmann::json({{"version",1},{"method","hybrid"},{"success",ok},
                {"cancelled",false},{"timeout",false},{"repairs",0},{"seed",headless_turn_config.generation_config.seed},
                {"profile",headless_turn_config.generation_config.profile_name},
                {"temperature",turn.effective_llm_temperature},
                {"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()},
                {"calls",nlohmann::json::array()},{"call_count",turn.llm_call_count},
                {"prompt_tokens",turn.prompt_tokens},{"generated_tokens",turn.generated_tokens},
                {"wall_ms",turn.llm_wall_time_ms},{"source",turn.generated_room_scene_source},
                {"fallback",turn.generated_room_metadata_fallback_used||turn.generated_room_scene_fallback_used},
                {"audit",{{"accepted",ok},{"triangles",generated.scene.triangles.size()},{"materials",generated.scene.materials.size()},
                    {"diagnostics",nlohmann::json::array()}}},{"error",generated.error}}).dump(2);
        } else ok=liminal::GenerateDynamicScene(dynamic_context,headless_turn_config.generation_config,
            headless_turn_config.dynamic_scene_config,progress,&bench_progress,&generated);
        if(!WriteFileText((scene_artifacts+".metrics.json").c_str(),generated.metrics_json) ||
           !WriteFileText((scene_artifacts+".design.json").c_str(),generated.plan_json) ||
           !WriteFileText((scene_artifacts+".scene").c_str(),generated.scene_text))return 1;
        for(size_t i=0;i<generated.attempts.size();++i) {
            const auto& a=generated.attempts[i];const auto stem=scene_artifacts+".attempt-"+std::to_string(i+1);
            if(!WriteFileText((stem+".raw.txt").c_str(),a.raw)||!WriteFileText((stem+".scene").c_str(),a.final_program)||
               !WriteFileText((stem+".json").c_str(),nlohmann::json({{"seed",a.seed},{"normalizations",a.normalizations},
                   {"normalized",a.normalized},{"removed_objects",a.removed_objects},{"repair_programs",a.repair_programs},
                   {"audit",nlohmann::json::parse(liminal::SerializeSceneAudit(a.after))},{"error",a.error}}).dump(2)))return 1;
        }
        if(!ok){fprintf(stderr,"Scene generation rejected: %s\n",generated.error.c_str());return 1;}
        const auto render_start=std::chrono::steady_clock::now();
        const bool rendered=liminal::RenderSceneToImage(generated.scene,config,output_path);
        auto metrics=nlohmann::json::parse(generated.metrics_json);
        metrics["final_render_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-render_start).count();
        metrics["success"]=rendered;
        if(!rendered)metrics["error"]="Final rendering or PNG writing failed";
        if(!WriteFileText((scene_artifacts+".metrics.json").c_str(),metrics.dump(2)))return 1;
        return rendered?0:1;
    }
    if (initial_state_output) {
        liminal::SessionState initial;
        if (!liminal::InitializeSessionState(selected_location, &initial, error_buffer, sizeof(error_buffer)) ||
            !SaveSessionStateToPath(initial_state_output, initial, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer); return 1;
        }
        return 0;
    }
    if (llm_probe_path) {
        std::string source;
        if (!ReadFileText(llm_probe_path, &source)) { fprintf(stderr, "Cannot read probe input\n"); return 1; }
        const auto input = nlohmann::json::parse(source, nullptr, false);
        if (!input.is_array()) { fprintf(stderr, "Probe input must be an array of role/content messages\n"); return 1; }
        std::vector<liminal::LlmPromptMessage> messages;
        for (const auto& item : input) {
            if (!item.is_object() || !item.contains("role") || !item["role"].is_string() ||
                !item.contains("content") || !item["content"].is_string()) return 1;
            messages.push_back({item["role"].get<std::string>(), item["content"].get<std::string>()});
        }
        if (llm_probe_schema) {
            if (!ReadFileText(llm_probe_schema, &headless_turn_config.generation_config.json_schema)) return 1;
            headless_turn_config.generation_config.use_json_grammar = true;
        }
        auto probe_outputs = nlohmann::json::array();
        bool all_ok = true;
        for (int repetition=0; repetition<llm_probe_repeat; ++repetition) {
        liminal::LlmGenerationResult result;
        struct ProbeStream {
            int remaining; int checks; bool during_load; liminal::LlmGenerationResult* result;
            int polls = 0; std::string final; bool leaked = false;
        } stream{llm_cancel_after, llm_cancel_after_checks, llm_cancel_during_load, &result};
        headless_turn_config.generation_config.purpose = "probe";
        const bool ok = liminal::GenerateChatCompletion(headless_turn_config.generation_config, messages,
            [](const char* accumulated, const char* delta, void* data) {
                auto* probe = static_cast<ProbeStream*>(data);
                ++probe->polls;
                if (probe->during_load && probe->polls > 1 && probe->result->load_time_ms == 0) return false;
                if (probe->result->generated_tokens > 0 || (accumulated && *accumulated)) {
                    if (probe->checks == 0) return false;
                    if (probe->checks > 0) --probe->checks;
                }
                probe->final = accumulated;
                if (probe->final.find("<|channel>") != std::string::npos || probe->final.find("<channel|>") != std::string::npos) probe->leaked = true;
                if (probe->remaining == 0) return false;
                if (probe->remaining > 0 && delta[0]) --probe->remaining;
                return true;
            }, &stream, &result);
        nlohmann::json output = {{"success",ok},{"final",result.response_text},{"raw",result.raw_text},
            {"thought",result.reasoning_text},{"prompt",result.prompt_text},{"stop_reason",result.stop_reason},
            {"error",result.error_message},{"stream_final",stream.final},{"stream_leaked_marker",stream.leaked},
            {"reasoning_tokens",result.reasoning_tokens},{"final_tokens",result.final_tokens},
            {"wall_time_ms",result.wall_time_ms},{"offloaded_layers",result.offloaded_layers}};
        output["model_reused"] = result.model_reused;
        output["load_time_ms"] = result.load_time_ms;
        output["prompt_time_ms"] = result.prompt_time_ms;
        output["generation_time_ms"] = result.generation_time_ms;
        probe_outputs.push_back(output);
        all_ok = all_ok && ok;
        }
        const auto output = llm_probe_repeat==1 ? probe_outputs[0] : probe_outputs;
        if (llm_probe_output && !WriteFileText(llm_probe_output, output.dump(2))) return 1;
        printf("%s\n", output.dump(2).c_str());
        return all_ok ? 0 : 2;
    }
    if (print_llama_info_only) {
        liminal::PrintLlmRuntimeInfo(stdout);
        liminal::ShutdownLlmRuntime();
        return 0;
    }

    if (run_animated_view_self_test) {
        return liminal::RunAnimatedViewSelfTest(stdout) ? 0 : 1;
    }
    if (run_navigation_self_test) {
        return liminal::RunNavigationSelfTest(stdout) ? 0 : 1;
    }
    if (run_view_post_process_self_test) {
        return liminal::RunPeripheralViewPostProcessSelfTest(stdout) ? 0 : 1;
    }

    if (dump_turn_contract || dump_generated_room_prompt || dump_scene_audit_prompt) {
        liminal::SessionState session_state;
        if (load_state_path) {
            if (!LoadSessionStateFromPath(load_state_path, &session_state, error_buffer, sizeof(error_buffer))) {
                fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot load session state.");
                return 1;
            }
        } else if (!liminal::InitializeSessionState(selected_location, &session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot initialize session state.");
            return 1;
        }

        if (dump_turn_contract) {
            printf(
                "%s\n",
                liminal::BuildTurnPrompt(
                    session_state.language,
                    session_state.hard_state,
                    session_state.soft_state,
                    session_state.spatial_state,
                    &session_state.history,
                    player_command,
                    true)
                    .c_str());
        } else if (dump_generated_room_prompt) {
            liminal::SpatialState prospective_spatial_state;
            prospective_spatial_state.location_id = liminal::kLocationUnknown;
            prospective_spatial_state.time_of_day = session_state.spatial_state.time_of_day;
            prospective_spatial_state.visibility_level = session_state.spatial_state.visibility_level;
            prospective_spatial_state.desert_state = session_state.spatial_state.desert_state;
            prospective_spatial_state.interior_density = session_state.spatial_state.interior_density;
            prospective_spatial_state.alert_level = session_state.spatial_state.alert_level > 0
                ? session_state.spatial_state.alert_level
                : session_state.hard_state.alert_level;
            liminal::AssignWorldPoseFromTraversal(
                session_state,
                session_state.current_place_id,
                generated_room_direction,
                &prospective_spatial_state);
            printf(
                "%s\n",
                liminal::BuildGeneratedRoomPrompt(
                    session_state.language,
                    session_state.hard_state,
                    session_state.soft_state,
                    session_state.spatial_state,
                    prospective_spatial_state,
                    &session_state.history,
                    generated_room_direction)
                    .c_str());
        } else {
            printf("%s\n", liminal::BuildSceneAuditPrompt(session_state.spatial_state).c_str());
        }
        return 0;
    }

    if (run_sdl) {
        liminal::SessionState session_state;
        if (load_state_path) {
            fprintf(stderr,"Every game starts fresh. --load-state is reserved for headless research diagnostics.\n");return 1;
        }
        if (!llm_seed_explicit) headless_turn_config.generation_config.seed = std::random_device{}() & 0x7fffffffu;
        if (!liminal::InitializeSessionState(selected_location, &session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot initialize session state.");
            return 1;
        }

#if defined(LIMINAL_HAVE_SDL3)
        liminal::SdlFrontendConfig sdl_config;
        if(research_root)sdl_config.research_root=research_root;
        sdl_config.render_config = config;
        sdl_config.turn_config = headless_turn_config;
        sdl_config.turn_config.prefer_candidate_scene = prefer_candidate_scene;
        sdl_config.animated_view_config.playback_images_per_second = view_animation_fps;
        sdl_config.animated_view_config.forced_failure_image_index = animated_view_fail_frame;
        sdl_config.animated_view_config.debug_logging = animated_view_debug;
        sdl_config.post_process_config = view_post_process_config;
        sdl_config.automated_smoke_test_duration_ms = sdl_smoke_test_duration_ms;
        sdl_config.automated_smoke_test_language = sdl_smoke_test_language;
        if (sdl_smoke_test_command) {
            sdl_config.automated_smoke_test_command = sdl_smoke_test_command;
        }

        liminal::SessionState final_session_state;
        if (!liminal::RunSdlFrontend(sdl_config, session_state, &final_session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "SDL frontend failed.");
            liminal::ShutdownLlmRuntime();
            return 1;
        }

        if (save_state_path && !SaveSessionStateToPath(save_state_path, final_session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot save session state.");
            liminal::ShutdownLlmRuntime();
            return 1;
        }

        if (dump_session_state) {
            liminal::PrintSessionStateSummary(final_session_state, stdout);
        }
        if (dump_session_history) {
            if (dump_session_state) {
                printf("\n");
            }
            liminal::PrintSessionHistory(final_session_state, stdout);
        }

        liminal::ShutdownLlmRuntime();
        return 0;
#else
        fprintf(stderr, "This binary was built without SDL3 support.\n");
        return 1;
#endif
    }

    const std::chrono::steady_clock::time_point load_start = std::chrono::steady_clock::now();
    if (audit_scene_view_path) {
        std::string program;
        if(!ReadFileText(audit_scene_view_path,&program))return 1;
        // These thresholds belong to the fixed-camera direct benchmark only.
        scene_view.aspect=static_cast<float>(config.width)/config.height;
        scene_view.near_distance=3.0f;scene_view.max_near_coverage=.9f;
        scene_view.require_return_clearance=true;
        liminal::SceneAuditReport report;
        const bool accepted=liminal::AuditDynamicScene(program,dynamic_context,nullptr,&scene,&report,&scene_view);
        if(!WriteFileText((scene_artifacts+".audit.json").c_str(),liminal::SerializeSceneAudit(report)))return 1;
        if(!accepted){fprintf(stderr,"Scene view rejected; see audit JSON\n");return 1;}
    } else if (audit_scene_text_path) {
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
        liminal::SessionState session_state;
        if (load_state_path) {
            if (!LoadSessionStateFromPath(load_state_path, &session_state, error_buffer, sizeof(error_buffer))) {
                fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot load session state.");
                return 1;
            }
        } else if (!liminal::InitializeSessionState(selected_location, &session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot initialize session state.");
            return 1;
        }

        headless_turn_config.prefer_candidate_scene = prefer_candidate_scene;
        liminal::HeadlessTurnResult turn_result;
        const bool turn_ready=liminal::RunHeadlessTurnFromState(session_state, player_command, headless_turn_config, &turn_result, error_buffer, sizeof(error_buffer));
        if(scene_artifacts_requested) {
            const auto trace=nlohmann::json({{"accepted",turn_ready},{"error",error_buffer},
                {"from",turn_result.initial_place_id},{"to",turn_result.updated_place_id},
                {"request",turn_result.request_text},{"prompt",turn_result.prompt_text},
                {"narrative_metrics",turn_result.narrative_metrics_json},{"narration",turn_result.turn_result.narration},
                {"llm_calls",turn_result.llm_call_count}}).dump(2);
            if(!WriteFileText((scene_artifacts+".turn.json").c_str(),trace))return 1;
        }
        // Explicit diagnostic export only; rejected artifacts never become a saved room.
        if(scene_artifacts_requested&&!turn_result.dynamic_scene_metrics_json.empty()) {
            if(!WriteFileText((scene_artifacts+".metrics.json").c_str(),turn_result.dynamic_scene_metrics_json)||
               !WriteFileText((scene_artifacts+".design.json").c_str(),turn_result.dynamic_scene_design_json)||
               !WriteFileText((scene_artifacts+".scene").c_str(),turn_result.raw_scene_audit_response_text))return 1;
        }
        if (!turn_ready) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Headless turn failed.");
            return 1;
        }
        liminal::UpdateSessionStateFromTurn(player_command, turn_result, &session_state);
        pending_saved_session=session_state;have_pending_save=true;
        scene = turn_result.rendered_scene;

        printf("Headless turn completed.\n");
        PrintTurnSummary(turn_result, dump_raw_turn);
        liminal::PrintHeadlessTurnDebugTrace(turn_result, stdout);
        if (dump_session_state) {
            printf("\n");
            liminal::PrintSessionStateSummary(session_state, stdout);
        }
        if (dump_session_history) {
            printf("\n");
            liminal::PrintSessionHistory(session_state, stdout);
        }
    } else if (run_session) {
        liminal::SessionState session_state;
        if (load_state_path) {
            if (!LoadSessionStateFromPath(load_state_path, &session_state, error_buffer, sizeof(error_buffer))) {
                fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot load session state.");
                return 1;
            }
        } else if (!liminal::InitializeSessionState(selected_location, &session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot initialize session state.");
            return 1;
        }

        if (session_commands.empty()) {
            session_commands.push_back(player_command);
        }

        headless_turn_config.prefer_candidate_scene = prefer_candidate_scene;
        int total_prompt_tokens = 0;
        int total_generated_tokens = 0;
        double total_inference_time_ms = 0.0;
        bool have_last_turn = false;
        liminal::HeadlessTurnResult last_turn_result;

        for (size_t index = 0; index < session_commands.size(); ++index) {
            liminal::HeadlessTurnResult turn_result;
            if (!liminal::RunHeadlessTurnFromState(
                    session_state,
                    session_commands[index].c_str(),
                    headless_turn_config,
                    &turn_result,
                    error_buffer,
                    sizeof(error_buffer))) {
                fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Headless session turn failed.");
                return 1;
            }

            printf("\n=== Session Turn %zu / %zu ===\n", index + 1, session_commands.size());
            PrintTurnSummary(turn_result, dump_raw_turn);
            liminal::PrintHeadlessTurnDebugTrace(turn_result, stdout);

            total_prompt_tokens += turn_result.prompt_tokens;
            total_generated_tokens += turn_result.generated_tokens;
            total_inference_time_ms += turn_result.inference_time_ms;
            scene = turn_result.rendered_scene;
            last_turn_result = turn_result;
            have_last_turn = true;
            liminal::UpdateSessionStateFromTurn(session_commands[index].c_str(), turn_result, &session_state);
        }

        if (!have_last_turn) {
            fprintf(stderr, "No session turn was executed.\n");
            return 1;
        }
        pending_saved_session=session_state;have_pending_save=true;

        printf("\nHeadless session completed.\n");
        printf("Turns: %zu\n", session_commands.size());
        printf("Total prompt tokens: %d\n", total_prompt_tokens);
        printf("Total generated tokens: %d\n", total_generated_tokens);
        printf("Total inference time: %.2f ms\n", total_inference_time_ms);
        printf("Final location: %s\n", liminal::LocationIdToString(session_state.spatial_state.location_id));
        printf("Final place: %s\n", liminal::DescribeCurrentPlaceLabel(session_state).c_str());
        printf(
            "Last rendered scene source: %s\n",
            last_turn_result.used_candidate_scene_for_render
                ? "candidate_scene_text"
                : (liminal::IsGeneratedPlaceId(last_turn_result.updated_place_id) ? "generated_room_cache" : "compiled_spatial_state"));
        if (dump_session_state) {
            printf("\n");
            liminal::PrintSessionStateSummary(session_state, stdout);
        }
        if (dump_session_history) {
            printf("\n");
            liminal::PrintSessionHistory(session_state, stdout);
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
        (run_turn || run_session) ? "Preparation time" : "Load time",
        std::chrono::duration<double, std::milli>(load_end - load_start).count());

    const std::chrono::steady_clock::time_point render_start = std::chrono::steady_clock::now();
    if (!liminal::RenderSceneToImage(scene, config, output_path)) {
        fprintf(stderr, "Rendering failed.\n");
        return 1;
    }
    const std::chrono::steady_clock::time_point render_end = std::chrono::steady_clock::now();
    if(have_pending_save && save_state_path &&
       !SaveSessionStateToPath(save_state_path,pending_saved_session,error_buffer,sizeof(error_buffer))) {
        fprintf(stderr,"%s\n",error_buffer);return 1;
    }

    printf(
        "Render time: %.2f ms\n",
        std::chrono::duration<double, std::milli>(render_end - render_start).count());
    printf("Wrote %s\n", output_path);
    liminal::ShutdownLlmRuntime();
    return 0;
}
