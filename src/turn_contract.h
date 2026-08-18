#ifndef LIMINAL_RENDERER_TURN_CONTRACT_H
#define LIMINAL_RENDERER_TURN_CONTRACT_H

#include <string>

#include "game_state.h"

namespace liminal {

std::string BuildTurnResultSchemaText();
std::string BuildGeneratedRoomSchemaText();
std::string BuildSceneFormatRuleText();
std::string BuildSpatialBriefText(const SpatialState& spatial_state);
std::string BuildTurnPrompt(
    GameLanguage language,
    const HardState& hard_state,
    const SoftState& soft_state,
    const SpatialState& spatial_state,
    const std::vector<SessionTurnRecord>* recent_history,
    const char* player_command,
    bool include_candidate_scene_text);
std::string BuildGeneratedRoomPrompt(
    GameLanguage language,
    const HardState& hard_state,
    const SoftState& soft_state,
    const SpatialState& current_spatial_state,
    const SpatialState& prospective_spatial_state,
    const std::vector<SessionTurnRecord>* recent_history,
    CardinalDirection direction);
std::string BuildGeneratedRoomScenePrompt(
    const SpatialState& current_spatial_state,
    const SpatialState& generated_spatial_state,
    CardinalDirection direction);
std::string BuildSceneAuditPrompt(const SpatialState& spatial_state);

}  // namespace liminal

#endif
