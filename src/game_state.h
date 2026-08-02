#ifndef LIMINAL_RENDERER_GAME_STATE_H
#define LIMINAL_RENDERER_GAME_STATE_H

#include <stddef.h>
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

enum CardinalDirection {
    kDirectionUnknown = 0,
    kDirectionNorth,
    kDirectionEast,
    kDirectionSouth,
    kDirectionWest,
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
    int move_count;
    int score;
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
        , move_count(0)
        , score(0)
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
    std::string room_title;
    std::string room_summary;
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
    std::vector<std::string> scene_constraints;

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
    bool move_count_changed;
    int next_move_count;
    bool score_changed;
    int next_score;
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
        , move_count_changed(false)
        , next_move_count(0)
        , score_changed(false)
        , next_score(0)
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
    bool anchors_present_changed;
    bool visible_objects_changed;
    bool blocked_exits_changed;
    bool spatial_anomalies_changed;
    bool scene_constraints_changed;
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
        , anchors_present_changed(false)
        , visible_objects_changed(false)
        , blocked_exits_changed(false)
        , spatial_anomalies_changed(false)
        , scene_constraints_changed(false)
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

struct SessionTurnRecord {
    int turn_number;
    LocationId location_id;
    std::string location_label;
    std::string player_command;
    std::string intent;
    std::string narration;
    std::string clarification;

    SessionTurnRecord()
        : turn_number(0)
        , location_id(kLocationUnknown)
    {
    }
};

struct GeneratedRoom {
    std::string room_id;
    SpatialState spatial_state;
    std::string scene_text;
    std::string scene_source;
    bool metadata_fallback_used;
    bool scene_fallback_used;

    GeneratedRoom()
        : metadata_fallback_used(false)
        , scene_fallback_used(false)
    {
    }
};

struct RoomLink {
    std::string from_place_id;
    CardinalDirection direction;
    std::string to_place_id;

    RoomLink()
        : direction(kDirectionUnknown)
    {
    }
};

struct SessionState {
    HardState hard_state;
    SoftState soft_state;
    SpatialState spatial_state;
    std::vector<SessionTurnRecord> history;
    std::string current_place_id;
    int next_generated_room_index;
    std::vector<GeneratedRoom> generated_rooms;
    std::vector<RoomLink> room_links;

    SessionState()
        : next_generated_room_index(1)
    {
    }
};

const char* LocationIdToString(LocationId value);
const char* CardinalDirectionToString(CardinalDirection value);
const char* TimeOfDayToString(TimeOfDay value);
const char* VisibilityLevelToString(VisibilityLevel value);
const char* DesertStateToString(DesertState value);
const char* InteriorDensityToString(InteriorDensity value);
const char* ResourceStateToString(ResourceState value);

bool ParseLocationId(const char* text, LocationId* value);
bool ParseCardinalDirection(const char* text, CardinalDirection* value);
bool ParseTimeOfDay(const char* text, TimeOfDay* value);
bool ParseVisibilityLevel(const char* text, VisibilityLevel* value);
bool ParseDesertState(const char* text, DesertState* value);
bool ParseInteriorDensity(const char* text, InteriorDensity* value);
bool ParseResourceState(const char* text, ResourceState* value);
CardinalDirection OppositeCardinalDirection(CardinalDirection value);
std::string BuildCanonicalPlaceId(LocationId location_id);
bool ParseCanonicalPlaceId(const std::string& place_id, LocationId* location_id);
bool IsCanonicalPlaceId(const std::string& place_id);
bool IsGeneratedPlaceId(const std::string& place_id);
std::string DescribePlaceLabel(const SessionState& state, const std::string& place_id);
std::string DescribeCurrentPlaceLabel(const SessionState& state);

HardState MakeInitialHardState();
SoftState MakeInitialSoftState();
void NormalizeSessionState(SessionState* state);
bool SerializeSessionStateToJsonString(const SessionState& state, std::string* json_text);
bool ParseSessionStateFromJson(
    const char* json_text,
    SessionState* state,
    char* error_buffer,
    size_t error_buffer_size);

void PrintHardStateSummary(const HardState& state, FILE* stream);
void PrintSoftStateSummary(const SoftState& state, FILE* stream);
void PrintSpatialStateSummary(const SpatialState& state, FILE* stream);
void PrintSessionStateSummary(const SessionState& state, FILE* stream);
void PrintSessionHistory(const SessionState& state, FILE* stream);

}  // namespace liminal

#endif
