#include "scene_compiler.h"

#include <stdio.h>
#include <string.h>

namespace liminal {

namespace {

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument);
}

static bool ReadTextFile(const char* path, std::string* text, char* error_buffer, size_t error_buffer_size)
{
    if (!path || !text) {
        SetError(error_buffer, error_buffer_size, "Invalid text path: %s", "(null)");
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        SetError(error_buffer, error_buffer_size, "Cannot open text file: %s", path);
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

static const char* CanonicalFixturePath(LocationId location_id)
{
    switch (location_id) {
    case kLocationGate:
        return "assets/scenes/datacenter_entry_gate.scene";
    case kLocationServerAisles:
        return "assets/scenes/datacenter_server_aisles.scene";
    case kLocationRoofWatch:
        return "assets/scenes/datacenter_roof_watch.scene";
    default:
        return 0;
    }
}

static void SetCommonGateSpatialState(SpatialState* state)
{
    state->room_title = "Entry Gate";
    state->room_summary = "A checkpoint threshold between the datacenter compound and the desert road.";
    state->location_archetype = "entry_threshold";
    state->canonical_fixture = CanonicalFixturePath(kLocationGate);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityDusty;
    state->desert_state = kDesertDusty;
    state->interior_density = kInteriorSparse;
    state->alert_level = 1;
    state->anchors.push_back("portal");
    state->anchors.push_back("fence");
    state->anchors.push_back("service_road");
    state->visible_objects.push_back("gate");
    state->visible_objects.push_back("lamp");
    state->visible_objects.push_back("checkpoint");
}

static void SetCommonAislesSpatialState(SpatialState* state)
{
    state->room_title = "Server Aisles";
    state->room_summary = "Dense rows of racks, maintenance lanes, cooling blocks and low night visibility.";
    state->location_archetype = "dense_server_interior";
    state->canonical_fixture = CanonicalFixturePath(kLocationServerAisles);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorDense;
    state->alert_level = 2;
    state->anchors.push_back("server_aisles");
    state->anchors.push_back("maintenance_lane");
    state->anchors.push_back("cooling_blocks");
    state->visible_objects.push_back("rack");
    state->visible_objects.push_back("cooling_unit");
    state->visible_objects.push_back("crate");
}

static void SetCommonRoofSpatialState(SpatialState* state)
{
    state->room_title = "Roof Watch";
    state->room_summary = "A parapet walk above the datacenter with a dark horizon and a dusty desert beyond.";
    state->location_archetype = "watch_post_exterior";
    state->canonical_fixture = CanonicalFixturePath(kLocationRoofWatch);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertDusty;
    state->interior_density = kInteriorSparse;
    state->alert_level = 2;
    state->anchors.push_back("parapet");
    state->anchors.push_back("horizon");
    state->anchors.push_back("roof_plant");
    state->visible_objects.push_back("parapet");
    state->visible_objects.push_back("cooling_unit");
    state->visible_objects.push_back("crate");
}

}  // namespace

bool BuildCanonicalSpatialState(LocationId location_id, SpatialState* spatial_state)
{
    if (!spatial_state) {
        return false;
    }

    *spatial_state = SpatialState();
    spatial_state->location_id = location_id;

    switch (location_id) {
    case kLocationGate:
        SetCommonGateSpatialState(spatial_state);
        return true;
    case kLocationServerAisles:
        SetCommonAislesSpatialState(spatial_state);
        return true;
    case kLocationRoofWatch:
        SetCommonRoofSpatialState(spatial_state);
        return true;
    default:
        spatial_state->location_archetype = "unknown";
        return false;
    }
}

bool CompileSpatialStateToScene(const SpatialState& spatial_state, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    if (!scene) {
        SetError(error_buffer, error_buffer_size, "Invalid compile target: %s", "(null)");
        return false;
    }

    const char* fixture_path = spatial_state.canonical_fixture.empty()
        ? CanonicalFixturePath(spatial_state.location_id)
        : spatial_state.canonical_fixture.c_str();
    if (!fixture_path || !fixture_path[0]) {
        SetError(error_buffer, error_buffer_size, "No canonical fixture for location: %s", LocationIdToString(spatial_state.location_id));
        return false;
    }

    std::string scene_text;
    if (!ReadTextFile(fixture_path, &scene_text, error_buffer, error_buffer_size)) {
        return false;
    }

    return LoadSceneFromSceneText(fixture_path, scene_text.c_str(), scene, error_buffer, error_buffer_size);
}

bool AuditSceneCandidateText(
    const char* scene_name,
    const char* scene_text,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    return LoadSceneFromSceneText(scene_name ? scene_name : "(candidate.scene)", scene_text, scene, error_buffer, error_buffer_size);
}

}  // namespace liminal
