#ifndef LIMINAL_RENDERER_SCENE_COMPILER_H
#define LIMINAL_RENDERER_SCENE_COMPILER_H

#include <stddef.h>
#include <string>

#include "game_state.h"
#include "scene.h"

namespace liminal {

bool BuildCanonicalSpatialState(LocationId location_id, SpatialState* spatial_state);
bool BuildSceneTextFromSpatialState(
    const SpatialState& spatial_state,
    std::string* scene_text,
    char* error_buffer,
    size_t error_buffer_size);
bool BuildSceneDebugReportFromSpatialState(
    const SpatialState& spatial_state,
    std::string* report_text,
    char* error_buffer,
    size_t error_buffer_size);
bool CompileSpatialStateToScene(const SpatialState& spatial_state, Scene* scene, char* error_buffer, size_t error_buffer_size);
bool AuditSceneCandidateText(
    const char* scene_name,
    const char* scene_text,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size);

}  // namespace liminal

#endif
