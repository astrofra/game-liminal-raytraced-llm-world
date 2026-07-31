#ifndef LIMINAL_RENDERER_GAME_STATE_H
#define LIMINAL_RENDERER_GAME_STATE_H

#include <stdio.h>
#include <string>
#include <vector>

namespace liminal {

enum LocationId {
    kLocationUnknown = 0,
    kLocationGate,
    kLocationServerAisles,
    kLocationRoofWatch,
};

enum TimeOfDay {
    kTimeUnknown = 0,
    kTimeDay,
    kTimeDusk,
    kTimeNight,
};

enum VisibilityLevel {
    kVisibilityUnknown = 0,
    kVisibilityClear,
    kVisibilityDusty,
    kVisibilityLow,
};

enum DesertState {
    kDesertUnknown = 0,
    kDesertStill,
    kDesertWindy,
    kDesertDusty,
};

enum InteriorDensity {
    kInteriorUnknown = 0,
    kInteriorSparse,
    kInteriorDense,
};

enum ResourceState {
    kResourceUnknown = 0,
    kResourceStable,
    kResourceStrained,
    kResourceCritical,
};

struct HardState {
    int turn_number;
    LocationId current_location_id;
    int alert_level;
    ResourceState cooling_state;
    ResourceState water_state;
    ResourceState power_state;
    std::vector<std::string> inventory_items;
    std::vector<std::string> named_entities;
    std::vector<std::string> unresolved_threats;

    HardState()
        : turn_number(0)
        , current_location_id(kLocationGate)
        , alert_level(1)
        , cooling_state(kResourceStable)
        , water_state(kResourceStable)
        , power_state(kResourceStable)
    {
    }
};

struct SoftState {
    std::string rolling_summary;
    std::string atmosphere;
    std::vector<std::string> active_hypotheses;
    std::vector<std::string> tolerated_incoherences;
};

struct SpatialState {
    LocationId location_id;
    std::string location_archetype;
    std::string canonical_fixture;
    TimeOfDay time_of_day;
    VisibilityLevel visibility_level;
    DesertState desert_state;
    InteriorDensity interior_density;
    int alert_level;
    std::vector<std::string> anchors;
    std::vector<std::string> visible_objects;
    std::vector<std::string> blocked_exits;
    std::vector<std::string> spatial_anomalies;

    SpatialState()
        : location_id(kLocationUnknown)
        , time_of_day(kTimeUnknown)
        , visibility_level(kVisibilityUnknown)
        , desert_state(kDesertUnknown)
        , interior_density(kInteriorUnknown)
        , alert_level(1)
    {
    }
};

struct HardStateDelta {
    bool location_changed;
    LocationId next_location_id;
    bool alert_level_changed;
    int next_alert_level;
    bool cooling_state_changed;
    ResourceState next_cooling_state;
    bool water_state_changed;
    ResourceState next_water_state;
    bool power_state_changed;
    ResourceState next_power_state;
    std::vector<std::string> inventory_add;
    std::vector<std::string> inventory_remove;
    std::vector<std::string> threats_add;
    std::vector<std::string> threats_remove;

    HardStateDelta()
        : location_changed(false)
        , next_location_id(kLocationUnknown)
        , alert_level_changed(false)
        , next_alert_level(0)
        , cooling_state_changed(false)
        , next_cooling_state(kResourceUnknown)
        , water_state_changed(false)
        , next_water_state(kResourceUnknown)
        , power_state_changed(false)
        , next_power_state(kResourceUnknown)
    {
    }
};

struct SpatialStateDelta {
    bool location_changed;
    LocationId next_location_id;
    bool time_of_day_changed;
    TimeOfDay next_time_of_day;
    bool visibility_changed;
    VisibilityLevel next_visibility_level;
    bool desert_state_changed;
    DesertState next_desert_state;
    bool interior_density_changed;
    InteriorDensity next_interior_density;
    bool alert_level_changed;
    int next_alert_level;
    std::vector<std::string> anchors_present;
    std::vector<std::string> visible_objects;
    std::vector<std::string> blocked_exits;
    std::vector<std::string> spatial_anomalies;
    std::vector<std::string> scene_constraints;

    SpatialStateDelta()
        : location_changed(false)
        , next_location_id(kLocationUnknown)
        , time_of_day_changed(false)
        , next_time_of_day(kTimeUnknown)
        , visibility_changed(false)
        , next_visibility_level(kVisibilityUnknown)
        , desert_state_changed(false)
        , next_desert_state(kDesertUnknown)
        , interior_density_changed(false)
        , next_interior_density(kInteriorUnknown)
        , alert_level_changed(false)
        , next_alert_level(0)
    {
    }
};

struct TurnResult {
    std::string intent;
    std::string narration;
    std::string clarification;
    HardStateDelta hard_state_delta;
    SpatialStateDelta spatial_delta;
    std::vector<std::string> continuity_notes;
    std::string candidate_scene_text;
    bool candidate_scene_included;

    TurnResult() : candidate_scene_included(false) {}
};

const char* LocationIdToString(LocationId value);
const char* TimeOfDayToString(TimeOfDay value);
const char* VisibilityLevelToString(VisibilityLevel value);
const char* DesertStateToString(DesertState value);
const char* InteriorDensityToString(InteriorDensity value);
const char* ResourceStateToString(ResourceState value);

bool ParseLocationId(const char* text, LocationId* value);
bool ParseTimeOfDay(const char* text, TimeOfDay* value);
bool ParseVisibilityLevel(const char* text, VisibilityLevel* value);
bool ParseDesertState(const char* text, DesertState* value);
bool ParseInteriorDensity(const char* text, InteriorDensity* value);
bool ParseResourceState(const char* text, ResourceState* value);

HardState MakeInitialHardState();
SoftState MakeInitialSoftState();

void PrintHardStateSummary(const HardState& state, FILE* stream);
void PrintSoftStateSummary(const SoftState& state, FILE* stream);
void PrintSpatialStateSummary(const SpatialState& state, FILE* stream);

}  // namespace liminal

#endif
