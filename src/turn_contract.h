#ifndef LIMINAL_RENDERER_TURN_CONTRACT_H
#define LIMINAL_RENDERER_TURN_CONTRACT_H

#include <string>

#include "game_state.h"

namespace liminal {

std::string BuildTurnResultSchemaText();
std::string BuildSceneFormatRuleText();
std::string BuildSpatialBriefText(const SpatialState& spatial_state);
std::string BuildTurnPrompt(
    const HardState& hard_state,
    const SoftState& soft_state,
    const SpatialState& spatial_state,
    const char* player_command,
    bool include_candidate_scene_text);
std::string BuildSceneAuditPrompt(const SpatialState& spatial_state);

}  // namespace liminal

#endif
