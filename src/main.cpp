#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "game_state.h"
#include "llm_runtime.h"
#include "renderer.h"
#include "sdl_frontend.h"
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
    printf("  --run-session            Run a multi-turn headless session through Ministral and render the last place\n");
    printf("  --sdl                    Launch the SDL3 interactive frontend with streaming LLM output\n");
    printf("  --model <path>           GGUF model path for --run-turn\n");
    printf("  --llm-temperature <f>    Sampling temperature for --run-turn\n");
    printf("  --llm-predict <n>        Maximum generated tokens for --run-turn\n");
    printf("  --use-json-grammar       Enable llama.cpp JSON grammar for --run-turn\n");
    printf("  --no-json-grammar        Disable llama.cpp JSON grammar for --run-turn\n");
    printf("  --command <text>         Player command; may be repeated for --run-session\n");
    printf("  --command-file <path>    Read one player command per non-empty line\n");
    printf("  --load-state <path>      Load session state JSON before --run-turn or --run-session\n");
    printf("  --save-state <path>      Save resulting session state JSON after --run-turn or --run-session\n");
    printf("  --dump-session-state     Print the final session state summary\n");
    printf("  --dump-session-history   Print the final session transcript summary\n");
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

static bool WriteFileText(const char* path, const std::string& text)
{
    if (!path) {
        return false;
    }

    FILE* file = fopen(path, "wb");
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
        turn_result.generated_room_scene_fallback_used ? "fallback" : "llm";

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
    if (!turn_result.generated_rooms_to_add.empty()) {
        printf("Generated room metadata source: %s\n", generated_room_metadata_source);
        printf("Generated room scene source: %s\n", generated_room_scene_source);
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
    bool run_session = false;
    bool run_sdl = false;
    bool prefer_candidate_scene = false;
    bool dump_raw_turn = false;
    bool dump_session_state = false;
    bool dump_session_history = false;
    const char* command_file_path = 0;
    const char* load_state_path = 0;
    const char* save_state_path = 0;
    liminal::LocationId selected_location = liminal::kLocationGate;
    liminal::HeadlessTurnConfig headless_turn_config;
    std::vector<std::string> session_commands;

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
        if (strcmp(argv[index], "--run-session") == 0) {
            run_session = true;
            continue;
        }
        if (strcmp(argv[index], "--sdl") == 0) {
            run_sdl = true;
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
    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    if ((run_turn && run_session) || (run_sdl && (run_turn || run_session))) {
        fprintf(stderr, "Choose either --run-turn, --run-session or --sdl.\n");
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

    if (print_llama_info_only) {
        liminal::PrintLlmRuntimeInfo(stdout);
        liminal::ShutdownLlmRuntime();
        return 0;
    }

    if (dump_turn_contract || dump_scene_audit_prompt) {
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
                    session_state.hard_state,
                    session_state.soft_state,
                    session_state.spatial_state,
                    &session_state.history,
                    player_command,
                    true)
                    .c_str());
        } else {
            printf("%s\n", liminal::BuildSceneAuditPrompt(session_state.spatial_state).c_str());
        }
        return 0;
    }

    if (run_sdl) {
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

#if defined(LIMINAL_HAVE_SDL3)
        liminal::SdlFrontendConfig sdl_config;
        sdl_config.render_config = config;
        sdl_config.turn_config = headless_turn_config;
        sdl_config.turn_config.prefer_candidate_scene = prefer_candidate_scene;

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
        if (!liminal::RunHeadlessTurnFromState(session_state, player_command, headless_turn_config, &turn_result, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Headless turn failed.");
            return 1;
        }
        liminal::UpdateSessionStateFromTurn(player_command, turn_result, &session_state);
        if (save_state_path && !SaveSessionStateToPath(save_state_path, session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot save session state.");
            return 1;
        }
        scene = turn_result.rendered_scene;

        printf("Headless turn completed.\n");
        PrintTurnSummary(turn_result, dump_raw_turn);
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
        if (save_state_path && !SaveSessionStateToPath(save_state_path, session_state, error_buffer, sizeof(error_buffer))) {
            fprintf(stderr, "%s\n", error_buffer[0] ? error_buffer : "Cannot save session state.");
            return 1;
        }

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

    printf(
        "Render time: %.2f ms\n",
        std::chrono::duration<double, std::milli>(render_end - render_start).count());
    printf("Wrote %s\n", output_path);
    liminal::ShutdownLlmRuntime();
    return 0;
}
