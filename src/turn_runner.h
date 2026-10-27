#ifndef LIMINAL_RENDERER_TURN_RUNNER_H
#define LIMINAL_RENDERER_TURN_RUNNER_H

#include <stddef.h>
#include <stdio.h>
#include <string>
#include <memory>

#include "game_state.h"
#include "llm_runtime.h"
#include "scene.h"
#include "dynamic_scene.h"

namespace liminal {

enum HeadlessTurnStreamPhase {
    kHeadlessTurnStreamPrimaryResponse = 0,
    kHeadlessTurnStreamRepairResponse,
    kHeadlessTurnStreamSceneProgram,
    kHeadlessTurnStreamSceneDesign,
    kHeadlessTurnStreamSceneAudit,
    kHeadlessTurnStreamScenePreview,
};

typedef bool (*HeadlessTurnStreamCallback)(
    HeadlessTurnStreamPhase phase,
    const char* accumulated_text,
    const char* delta_text,
    void* user_data);

struct HeadlessTurnConfig {
    std::shared_ptr<const AmbienceCatalog> ambience_catalog;
    AudioGains audio_current_gains{};
    bool audio_current_known = false;
    LlmGenerationConfig generation_config;
    bool dynamic_scenes = false;
    bool local_scenes = false; // Generate persistent places along open cardinal exits.
    bool investigation_opportunities = true; // Disable only for isolated geometry diagnostics/fixtures.
    bool reuse_unchanged_view = false; // Frontend owns the existing image; headless exports still render.
    DynamicSceneConfig dynamic_scene_config;
    SceneCompletion narrative_completion; // Optional deterministic seam for fiction tests.
    SceneCompletion interaction_completion; // Structured free-command interpretation, independently testable.
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
    InvestigationState initial_investigation, updated_investigation;
    bool investigation_prepared = false;
    bool investigation_placement_rejected = false;
    std::string investigation_metrics_json;
    AudioMixDecision audio_mix;
    ObjectState updated_objects;
    bool objects_prepared = false;
    int initial_object_revision = 0;
    std::string interaction_metrics_json;
    std::string authorized_object_action, authorized_object_id;
    std::string accepted_observation;
    bool accepted = false; // Set only after the full turn and last cancellation poll succeed.
    bool view_unchanged = false; // Read-only inventory response, no scene or pixels supplied.
    int llm_call_count = 0;
    int reasoning_tokens = 0;
    int final_tokens = 0;
    double llm_wall_time_ms = 0.0;
    std::string request_text;
    std::string prompt_text;
    std::string raw_response_text;
    std::string repair_response_text;
    std::string raw_scene_audit_response_text;
    std::string dynamic_scene_design_json;
    std::string dynamic_scene_metrics_json;
    std::vector<DynamicSceneResult::Attempt> scene_attempts;
    std::string narrative_metrics_json;
    std::string engine_outcome;
    std::string thermal_prompt_text;
    std::string thermal_response_text;
    std::string rendered_scene_text;
    std::string rendered_scene_debug_text;
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
    SpatialState prospective_spatial_state;
    CardinalDirection traversal_direction;
    bool traversal_requested;
    bool traversal_inferred_from_llm;
    bool prospective_spatial_state_known;
    bool candidate_scene_valid;
    bool used_candidate_scene_for_render;
    bool used_turn_repair;
    bool used_turn_fallback;
    bool generated_room_created;
    bool generated_room_cache_refreshed;
    bool generated_room_metadata_fallback_used;
    bool generated_room_scene_fallback_used;
    std::string generated_room_scene_source;
    std::string candidate_scene_error;
    std::vector<GeneratedRoom> generated_rooms_to_add;
    std::vector<RoomLink> room_links_to_add;
    std::vector<InvisibleBarrier> invisible_barriers_to_update;
    bool invisible_barrier_contact;
    bool thermal_update_used;
    bool player_text_localization_used;
    int prompt_tokens;
    int generated_tokens;
    double inference_time_ms;
    float effective_llm_temperature;

    HeadlessTurnResult()
        : traversal_direction(kDirectionUnknown)
        , traversal_requested(false)
        , traversal_inferred_from_llm(false)
        , prospective_spatial_state_known(false)
        , candidate_scene_valid(false)
        , used_candidate_scene_for_render(false)
        , used_turn_repair(false)
        , used_turn_fallback(false)
        , generated_room_created(false)
        , generated_room_cache_refreshed(false)
        , generated_room_metadata_fallback_used(false)
        , generated_room_scene_fallback_used(false)
        , invisible_barrier_contact(false)
        , thermal_update_used(false)
        , player_text_localization_used(false)
        , prompt_tokens(0)
        , generated_tokens(0)
        , inference_time_ms(0.0)
        , effective_llm_temperature(0.0f)
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
void RestoreAuthoritativeCanonicalTopology(SessionState* session_state);
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
void PrintHeadlessTurnDebugTrace(const HeadlessTurnResult& result, FILE* stream);
bool RunHeadlessTurn(
    LocationId initial_location_id,
    const char* player_command,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size);
bool RunNavigationSelfTest(FILE* output);

}  // namespace liminal

#endif
