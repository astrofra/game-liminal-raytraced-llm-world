#ifndef LIMINAL_RENDERER_TURN_RUNNER_H
#define LIMINAL_RENDERER_TURN_RUNNER_H

#include <stddef.h>
#include <string>

#include "game_state.h"
#include "llm_runtime.h"
#include "scene.h"

namespace liminal {

enum HeadlessTurnStreamPhase {
    kHeadlessTurnStreamPrimaryResponse = 0,
    kHeadlessTurnStreamRepairResponse,
    kHeadlessTurnStreamSceneProgram,
};

typedef bool (*HeadlessTurnStreamCallback)(
    HeadlessTurnStreamPhase phase,
    const char* accumulated_text,
    const char* delta_text,
    void* user_data);

struct HeadlessTurnConfig {
    LlmGenerationConfig generation_config;
    bool request_candidate_scene;
    bool prefer_candidate_scene;
    HeadlessTurnStreamCallback stream_callback;
    void* stream_user_data;

    HeadlessTurnConfig()
        : request_candidate_scene(true)
        , prefer_candidate_scene(false)
        , stream_callback(0)
        , stream_user_data(0)
    {
    }
};

struct HeadlessTurnResult {
    std::string prompt_text;
    std::string raw_response_text;
    std::string repair_response_text;
    std::string raw_scene_audit_response_text;
    std::string initial_place_id;
    std::string updated_place_id;
    TurnResult turn_result;
    HardState initial_hard_state;
    SoftState initial_soft_state;
    SpatialState initial_spatial_state;
    HardState updated_hard_state;
    SoftState updated_soft_state;
    SpatialState updated_spatial_state;
    Scene rendered_scene;
    Scene candidate_scene;
    bool candidate_scene_valid;
    bool used_candidate_scene_for_render;
    bool used_turn_repair;
    bool used_turn_fallback;
    bool generated_room_metadata_fallback_used;
    bool generated_room_scene_fallback_used;
    std::string candidate_scene_error;
    std::vector<GeneratedRoom> generated_rooms_to_add;
    std::vector<RoomLink> room_links_to_add;
    int prompt_tokens;
    int generated_tokens;
    double inference_time_ms;

    HeadlessTurnResult()
        : candidate_scene_valid(false)
        , used_candidate_scene_for_render(false)
        , used_turn_repair(false)
        , used_turn_fallback(false)
        , generated_room_metadata_fallback_used(false)
        , generated_room_scene_fallback_used(false)
        , prompt_tokens(0)
        , generated_tokens(0)
        , inference_time_ms(0.0)
    {
    }
};

bool ParseTurnResultJson(
    const char* json_text,
    TurnResult* turn_result,
    char* error_buffer,
    size_t error_buffer_size);
bool InitializeSessionState(
    LocationId initial_location_id,
    SessionState* session_state,
    char* error_buffer,
    size_t error_buffer_size);
void ApplyTurnResult(
    const TurnResult& turn_result,
    HardState* hard_state,
    SoftState* soft_state,
    SpatialState* spatial_state);
void UpdateSessionStateFromTurn(
    const char* player_command,
    const HeadlessTurnResult& turn_result,
    SessionState* session_state);
bool RunHeadlessTurnFromState(
    const SessionState& initial_session_state,
    const char* player_command,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size);
bool RunHeadlessTurn(
    LocationId initial_location_id,
    const char* player_command,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size);

}  // namespace liminal

#endif
