#include "game_state.h"

#include <math.h>
#include <string.h>

#include "nlohmann/json.hpp"

namespace liminal {

namespace {

using json = nlohmann::json;

static bool EqualsAsciiNoCase(const char* a, const char* b)
{
    if (!a || !b) {
        return false;
    }

    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static void PrintStringList(const char* label, const std::vector<std::string>& values, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "%s:", label);
    if (values.empty()) {
        fprintf(out, " (none)\n");
        return;
    }

    for (size_t index = 0; index < values.size(); ++index) {
        fprintf(out, "%s%s", index == 0 ? " " : ", ", values[index].c_str());
    }
    fprintf(out, "\n");
}

static std::vector<std::string> ReadStringArrayNode(const json& node)
{
    std::vector<std::string> values;
    if (!node.is_array()) {
        return values;
    }

    for (size_t index = 0; index < node.size(); ++index) {
        if (node[index].is_string()) {
            values.push_back(node[index].get<std::string>());
        }
    }
    return values;
}

static std::string ReadStringNode(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return std::string();
    }
    return object[key].get<std::string>();
}

static int ReadIntNode(const json& object, const char* key, int default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_number_integer()) {
        return default_value;
    }
    return object[key].get<int>();
}

static bool ReadBoolNode(const json& object, const char* key, bool default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_boolean()) {
        return default_value;
    }
    return object[key].get<bool>();
}

static float ReadFloatNode(const json& object, const char* key, float default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_number()) {
        return default_value;
    }
    return object[key].get<float>();
}

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument ? argument : "(null)");
}

static std::string BuildPlaceLabelFromSpatialState(const SpatialState& state)
{
    if (!state.room_title.empty()) {
        return state.room_title;
    }
    if (state.location_id != kLocationUnknown) {
        return LocationIdToString(state.location_id);
    }
    if (!state.location_archetype.empty()) {
        return state.location_archetype;
    }
    return "unknown";
}

static const float kHiddenWorldStep = 3.0f;
static const float kHiddenWorldCoreRadius = 2.5f;
static const float kHiddenWorldInnerRingRadius = 6.5f;
static const float kHiddenWorldPerimeterRadius = 10.5f;
static const float kHiddenWorldParapetRadius = 13.5f;
static const float kPiDegrees = 57.2957795130823208768f;

enum HiddenWorldBand {
    kHiddenWorldBandUnknown = 0,
    kHiddenWorldBandCentralCore,
    kHiddenWorldBandInnerTechnicalRing,
    kHiddenWorldBandPerimeterSeam,
    kHiddenWorldBandOuterParapet,
    kHiddenWorldBandOpenDesert,
};

static void ApplyTraversalStep(CardinalDirection direction, float* world_x, float* world_z)
{
    if (!world_x || !world_z) {
        return;
    }

    switch (direction) {
    case kDirectionNorth:
        *world_z += kHiddenWorldStep;
        break;
    case kDirectionEast:
        *world_x += kHiddenWorldStep;
        break;
    case kDirectionSouth:
        *world_z -= kHiddenWorldStep;
        break;
    case kDirectionWest:
        *world_x -= kHiddenWorldStep;
        break;
    default:
        break;
    }
}

static HiddenWorldBand ClassifyHiddenWorldBandFromRadius(float radius)
{
    if (radius < 0.0f) {
        return kHiddenWorldBandUnknown;
    }
    if (radius <= kHiddenWorldCoreRadius) {
        return kHiddenWorldBandCentralCore;
    }
    if (radius <= kHiddenWorldInnerRingRadius) {
        return kHiddenWorldBandInnerTechnicalRing;
    }
    if (radius <= kHiddenWorldPerimeterRadius) {
        return kHiddenWorldBandPerimeterSeam;
    }
    if (radius <= kHiddenWorldParapetRadius) {
        return kHiddenWorldBandOuterParapet;
    }
    return kHiddenWorldBandOpenDesert;
}

static json MakeHardStateJson(const HardState& state)
{
    json node = json::object();
    node["turn_number"] = state.turn_number;
    node["move_count"] = state.move_count;
    node["score"] = state.score;
    node["current_location_id"] = LocationIdToString(state.current_location_id);
    node["alert_level"] = state.alert_level;
    node["datacenter_temperature_c"] = ClampDatacenterTemperatureC(state.datacenter_temperature_c);
    node["cooling_state"] = ResourceStateToString(state.cooling_state);
    node["water_state"] = ResourceStateToString(state.water_state);
    node["power_state"] = ResourceStateToString(state.power_state);
    node["inventory_items"] = state.inventory_items;
    node["named_entities"] = state.named_entities;
    node["unresolved_threats"] = state.unresolved_threats;
    return node;
}

static json MakeSoftStateJson(const SoftState& state)
{
    json node = json::object();
    node["rolling_summary"] = state.rolling_summary;
    node["atmosphere"] = state.atmosphere;
    node["active_hypotheses"] = state.active_hypotheses;
    node["tolerated_incoherences"] = state.tolerated_incoherences;
    return node;
}

static json MakeSpatialStateJson(const SpatialState& state)
{
    json node = json::object();
    node["location_id"] = LocationIdToString(state.location_id);
    node["room_title"] = state.room_title;
    node["room_summary"] = state.room_summary;
    node["location_archetype"] = state.location_archetype;
    node["canonical_fixture"] = state.canonical_fixture;
    node["world_pose_known"] = state.world_pose_known;
    node["world_x"] = state.world_x;
    node["world_z"] = state.world_z;
    node["time_of_day"] = TimeOfDayToString(state.time_of_day);
    node["visibility_level"] = VisibilityLevelToString(state.visibility_level);
    node["desert_state"] = DesertStateToString(state.desert_state);
    node["interior_density"] = InteriorDensityToString(state.interior_density);
    node["alert_level"] = state.alert_level;
    node["anchors"] = state.anchors;
    node["visible_objects"] = state.visible_objects;
    node["blocked_exits"] = state.blocked_exits;
    node["spatial_anomalies"] = state.spatial_anomalies;
    node["scene_constraints"] = state.scene_constraints;
    return node;
}

static json MakeSessionHistoryJson(const std::vector<SessionTurnRecord>& history)
{
    json node = json::array();
    for (size_t index = 0; index < history.size(); ++index) {
        const SessionTurnRecord& record = history[index];
        json item = json::object();
        item["turn_number"] = record.turn_number;
        item["location_id"] = LocationIdToString(record.location_id);
        item["location_label"] = record.location_label;
        item["player_command"] = record.player_command;
        item["intent"] = record.intent;
        item["narration"] = record.narration;
        item["clarification"] = record.clarification;
        node.push_back(item);
    }
    return node;
}

static json MakeGeneratedRoomsJson(const std::vector<GeneratedRoom>& rooms)
{
    json node = json::array();
    for (size_t index = 0; index < rooms.size(); ++index) {
        json item = json::object();
        item["room_id"] = rooms[index].room_id;
        item["spatial_state"] = MakeSpatialStateJson(rooms[index].spatial_state);
        item["scene_text"] = rooms[index].scene_text;
        item["scene_source"] = rooms[index].scene_source;
        item["metadata_fallback_used"] = rooms[index].metadata_fallback_used;
        item["scene_fallback_used"] = rooms[index].scene_fallback_used;
        node.push_back(item);
    }
    return node;
}

static json MakeRoomLinksJson(const std::vector<RoomLink>& links)
{
    json node = json::array();
    for (size_t index = 0; index < links.size(); ++index) {
        json item = json::object();
        item["from_place_id"] = links[index].from_place_id;
        item["direction"] = CardinalDirectionToString(links[index].direction);
        item["to_place_id"] = links[index].to_place_id;
        node.push_back(item);
    }
    return node;
}

static void ParseHardStateNode(const json& node, HardState* state)
{
    if (!state || !node.is_object()) {
        return;
    }

    state->turn_number = ReadIntNode(node, "turn_number", state->turn_number);
    state->move_count = ReadIntNode(node, "move_count", state->move_count);
    state->score = ReadIntNode(node, "score", state->score);
    ParseLocationId(ReadStringNode(node, "current_location_id").c_str(), &state->current_location_id);
    state->alert_level = ReadIntNode(node, "alert_level", state->alert_level);
    state->datacenter_temperature_c =
        ClampDatacenterTemperatureC(
            ReadIntNode(node, "datacenter_temperature_c", state->datacenter_temperature_c));
    ParseResourceState(ReadStringNode(node, "cooling_state").c_str(), &state->cooling_state);
    ParseResourceState(ReadStringNode(node, "water_state").c_str(), &state->water_state);
    ParseResourceState(ReadStringNode(node, "power_state").c_str(), &state->power_state);
    if (node.contains("inventory_items")) {
        state->inventory_items = ReadStringArrayNode(node["inventory_items"]);
    }
    if (node.contains("named_entities")) {
        state->named_entities = ReadStringArrayNode(node["named_entities"]);
    }
    if (node.contains("unresolved_threats")) {
        state->unresolved_threats = ReadStringArrayNode(node["unresolved_threats"]);
    }
}

static void ParseSoftStateNode(const json& node, SoftState* state)
{
    if (!state || !node.is_object()) {
        return;
    }

    state->rolling_summary = ReadStringNode(node, "rolling_summary");
    state->atmosphere = ReadStringNode(node, "atmosphere");
    if (node.contains("active_hypotheses")) {
        state->active_hypotheses = ReadStringArrayNode(node["active_hypotheses"]);
    }
    if (node.contains("tolerated_incoherences")) {
        state->tolerated_incoherences = ReadStringArrayNode(node["tolerated_incoherences"]);
    }
}

static void ParseSpatialStateNode(const json& node, SpatialState* state)
{
    if (!state || !node.is_object()) {
        return;
    }

    ParseLocationId(ReadStringNode(node, "location_id").c_str(), &state->location_id);
    state->room_title = ReadStringNode(node, "room_title");
    state->room_summary = ReadStringNode(node, "room_summary");
    state->location_archetype = ReadStringNode(node, "location_archetype");
    state->canonical_fixture = ReadStringNode(node, "canonical_fixture");
    state->world_pose_known = ReadBoolNode(node, "world_pose_known", state->world_pose_known);
    state->world_x = ReadFloatNode(node, "world_x", state->world_x);
    state->world_z = ReadFloatNode(node, "world_z", state->world_z);
    ParseTimeOfDay(ReadStringNode(node, "time_of_day").c_str(), &state->time_of_day);
    ParseVisibilityLevel(ReadStringNode(node, "visibility_level").c_str(), &state->visibility_level);
    ParseDesertState(ReadStringNode(node, "desert_state").c_str(), &state->desert_state);
    ParseInteriorDensity(ReadStringNode(node, "interior_density").c_str(), &state->interior_density);
    state->alert_level = ReadIntNode(node, "alert_level", state->alert_level);
    if (node.contains("anchors")) {
        state->anchors = ReadStringArrayNode(node["anchors"]);
    }
    if (node.contains("visible_objects")) {
        state->visible_objects = ReadStringArrayNode(node["visible_objects"]);
    }
    if (node.contains("blocked_exits")) {
        state->blocked_exits = ReadStringArrayNode(node["blocked_exits"]);
    }
    if (node.contains("spatial_anomalies")) {
        state->spatial_anomalies = ReadStringArrayNode(node["spatial_anomalies"]);
    }
    if (node.contains("scene_constraints")) {
        state->scene_constraints = ReadStringArrayNode(node["scene_constraints"]);
    }
}

static void ParseHistoryNode(const json& node, std::vector<SessionTurnRecord>* history)
{
    if (!history || !node.is_array()) {
        return;
    }

    history->clear();
    for (size_t index = 0; index < node.size(); ++index) {
        if (!node[index].is_object()) {
            continue;
        }

        SessionTurnRecord record;
        record.turn_number = ReadIntNode(node[index], "turn_number", 0);
        ParseLocationId(ReadStringNode(node[index], "location_id").c_str(), &record.location_id);
        record.location_label = ReadStringNode(node[index], "location_label");
        record.player_command = ReadStringNode(node[index], "player_command");
        record.intent = ReadStringNode(node[index], "intent");
        record.narration = ReadStringNode(node[index], "narration");
        record.clarification = ReadStringNode(node[index], "clarification");
        history->push_back(record);
    }
}

static void ParseGeneratedRoomsNode(const json& node, std::vector<GeneratedRoom>* rooms)
{
    if (!rooms || !node.is_array()) {
        return;
    }

    rooms->clear();
    for (size_t index = 0; index < node.size(); ++index) {
        if (!node[index].is_object()) {
            continue;
        }

        GeneratedRoom room;
        room.room_id = ReadStringNode(node[index], "room_id");
        if (node[index].contains("spatial_state")) {
            ParseSpatialStateNode(node[index]["spatial_state"], &room.spatial_state);
        }
        room.scene_text = ReadStringNode(node[index], "scene_text");
        room.scene_source = ReadStringNode(node[index], "scene_source");
        room.metadata_fallback_used = ReadBoolNode(node[index], "metadata_fallback_used", false);
        room.scene_fallback_used = ReadBoolNode(node[index], "scene_fallback_used", false);
        if (room.scene_source.empty()) {
            room.scene_source = room.scene_fallback_used ? "fallback" : "llm";
        }
        if (!room.room_id.empty()) {
            rooms->push_back(room);
        }
    }
}

static void ParseRoomLinksNode(const json& node, std::vector<RoomLink>* links)
{
    if (!links || !node.is_array()) {
        return;
    }

    links->clear();
    for (size_t index = 0; index < node.size(); ++index) {
        if (!node[index].is_object()) {
            continue;
        }

        RoomLink link;
        link.from_place_id = ReadStringNode(node[index], "from_place_id");
        ParseCardinalDirection(ReadStringNode(node[index], "direction").c_str(), &link.direction);
        link.to_place_id = ReadStringNode(node[index], "to_place_id");
        if (!link.from_place_id.empty() && !link.to_place_id.empty() && link.direction != kDirectionUnknown) {
            links->push_back(link);
        }
    }
}

static bool VectorContainsString(const std::vector<std::string>& values, const std::string& value)
{
    for (size_t index = 0; index < values.size(); ++index) {
        if (values[index] == value) {
            return true;
        }
    }
    return false;
}

static bool IsKnownPlaceIdInSession(const SessionState& state, const std::string& place_id)
{
    if (place_id.empty()) {
        return false;
    }

    LocationId location_id = kLocationUnknown;
    if (ParseCanonicalPlaceId(place_id, &location_id)) {
        return true;
    }

    if (!IsGeneratedPlaceId(place_id)) {
        return false;
    }

    for (size_t index = 0; index < state.generated_rooms.size(); ++index) {
        if (state.generated_rooms[index].room_id == place_id) {
            return true;
        }
    }
    return false;
}

static bool TryInferWorldPoseFromLinks(const SessionState& state, const std::string& place_id, float* world_x, float* world_z)
{
    if (!world_x || !world_z || place_id.empty()) {
        return false;
    }

    for (size_t index = 0; index < state.room_links.size(); ++index) {
        const RoomLink& link = state.room_links[index];
        float base_x = 0.0f;
        float base_z = 0.0f;
        if (link.to_place_id == place_id && ResolvePlaceWorldPose(state, link.from_place_id, &base_x, &base_z)) {
            ApplyTraversalStep(link.direction, &base_x, &base_z);
            *world_x = base_x;
            *world_z = base_z;
            return true;
        }
        if (link.from_place_id == place_id && ResolvePlaceWorldPose(state, link.to_place_id, &base_x, &base_z)) {
            ApplyTraversalStep(OppositeCardinalDirection(link.direction), &base_x, &base_z);
            *world_x = base_x;
            *world_z = base_z;
            return true;
        }
    }

    return false;
}

}  // namespace

int ClampDatacenterTemperatureC(int value)
{
    if (value < kMinDatacenterTemperatureC) {
        return kMinDatacenterTemperatureC;
    }
    if (value > kMaxDatacenterTemperatureC) {
        return kMaxDatacenterTemperatureC;
    }
    return value;
}

const char* LocationIdToString(LocationId value)
{
    switch (value) {
    case kLocationGate:
        return "gate";
    case kLocationServerAisles:
        return "server_aisles";
    case kLocationRoofWatch:
        return "roof_watch";
    default:
        return "unknown";
    }
}

const char* CardinalDirectionToString(CardinalDirection value)
{
    switch (value) {
    case kDirectionNorth:
        return "north";
    case kDirectionEast:
        return "east";
    case kDirectionSouth:
        return "south";
    case kDirectionWest:
        return "west";
    default:
        return "unknown";
    }
}

const char* TimeOfDayToString(TimeOfDay value)
{
    switch (value) {
    case kTimeDay:
        return "day";
    case kTimeDusk:
        return "dusk";
    case kTimeNight:
        return "night";
    default:
        return "unknown";
    }
}

const char* VisibilityLevelToString(VisibilityLevel value)
{
    switch (value) {
    case kVisibilityClear:
        return "clear";
    case kVisibilityDusty:
        return "dusty";
    case kVisibilityLow:
        return "low";
    default:
        return "unknown";
    }
}

const char* DesertStateToString(DesertState value)
{
    switch (value) {
    case kDesertStill:
        return "still";
    case kDesertWindy:
        return "windy";
    case kDesertDusty:
        return "dusty";
    default:
        return "unknown";
    }
}

const char* InteriorDensityToString(InteriorDensity value)
{
    switch (value) {
    case kInteriorSparse:
        return "sparse";
    case kInteriorDense:
        return "dense";
    default:
        return "unknown";
    }
}

const char* ResourceStateToString(ResourceState value)
{
    switch (value) {
    case kResourceStable:
        return "stable";
    case kResourceStrained:
        return "strained";
    case kResourceCritical:
        return "critical";
    default:
        return "unknown";
    }
}

bool ParseLocationId(const char* text, LocationId* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "gate")) {
        *value = kLocationGate;
        return true;
    }
    if (EqualsAsciiNoCase(text, "server_aisles") || EqualsAsciiNoCase(text, "server-aisles")) {
        *value = kLocationServerAisles;
        return true;
    }
    if (EqualsAsciiNoCase(text, "roof_watch") || EqualsAsciiNoCase(text, "roof-watch") || EqualsAsciiNoCase(text, "roof")) {
        *value = kLocationRoofWatch;
        return true;
    }

    *value = kLocationUnknown;
    return false;
}

bool ParseCardinalDirection(const char* text, CardinalDirection* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "north") || EqualsAsciiNoCase(text, "n")) {
        *value = kDirectionNorth;
        return true;
    }
    if (EqualsAsciiNoCase(text, "east") || EqualsAsciiNoCase(text, "e")) {
        *value = kDirectionEast;
        return true;
    }
    if (EqualsAsciiNoCase(text, "south") || EqualsAsciiNoCase(text, "s")) {
        *value = kDirectionSouth;
        return true;
    }
    if (EqualsAsciiNoCase(text, "west") || EqualsAsciiNoCase(text, "w")) {
        *value = kDirectionWest;
        return true;
    }

    *value = kDirectionUnknown;
    return false;
}

bool ParseTimeOfDay(const char* text, TimeOfDay* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "day")) {
        *value = kTimeDay;
        return true;
    }
    if (EqualsAsciiNoCase(text, "dusk")) {
        *value = kTimeDusk;
        return true;
    }
    if (EqualsAsciiNoCase(text, "night")) {
        *value = kTimeNight;
        return true;
    }

    *value = kTimeUnknown;
    return false;
}

bool ParseVisibilityLevel(const char* text, VisibilityLevel* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "clear")) {
        *value = kVisibilityClear;
        return true;
    }
    if (EqualsAsciiNoCase(text, "dusty")) {
        *value = kVisibilityDusty;
        return true;
    }
    if (EqualsAsciiNoCase(text, "low")) {
        *value = kVisibilityLow;
        return true;
    }

    *value = kVisibilityUnknown;
    return false;
}

bool ParseDesertState(const char* text, DesertState* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "still")) {
        *value = kDesertStill;
        return true;
    }
    if (EqualsAsciiNoCase(text, "windy")) {
        *value = kDesertWindy;
        return true;
    }
    if (EqualsAsciiNoCase(text, "dusty")) {
        *value = kDesertDusty;
        return true;
    }

    *value = kDesertUnknown;
    return false;
}

bool ParseInteriorDensity(const char* text, InteriorDensity* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "sparse")) {
        *value = kInteriorSparse;
        return true;
    }
    if (EqualsAsciiNoCase(text, "dense")) {
        *value = kInteriorDense;
        return true;
    }

    *value = kInteriorUnknown;
    return false;
}

bool ParseResourceState(const char* text, ResourceState* value)
{
    if (!text || !value) {
        return false;
    }

    if (EqualsAsciiNoCase(text, "stable")) {
        *value = kResourceStable;
        return true;
    }
    if (EqualsAsciiNoCase(text, "strained")) {
        *value = kResourceStrained;
        return true;
    }
    if (EqualsAsciiNoCase(text, "critical")) {
        *value = kResourceCritical;
        return true;
    }

    *value = kResourceUnknown;
    return false;
}

CardinalDirection OppositeCardinalDirection(CardinalDirection value)
{
    switch (value) {
    case kDirectionNorth:
        return kDirectionSouth;
    case kDirectionEast:
        return kDirectionWest;
    case kDirectionSouth:
        return kDirectionNorth;
    case kDirectionWest:
        return kDirectionEast;
    default:
        return kDirectionUnknown;
    }
}

std::string BuildCanonicalPlaceId(LocationId location_id)
{
    return std::string("canonical:") + LocationIdToString(location_id);
}

bool ParseCanonicalPlaceId(const std::string& place_id, LocationId* location_id)
{
    if (!location_id) {
        return false;
    }

    const std::string prefix = "canonical:";
    if (place_id.compare(0, prefix.size(), prefix) != 0) {
        *location_id = kLocationUnknown;
        return false;
    }

    return ParseLocationId(place_id.substr(prefix.size()).c_str(), location_id);
}

bool IsCanonicalPlaceId(const std::string& place_id)
{
    LocationId location_id = kLocationUnknown;
    return ParseCanonicalPlaceId(place_id, &location_id);
}

bool IsGeneratedPlaceId(const std::string& place_id)
{
    return place_id.compare(0, 10, "generated:") == 0;
}

std::string DescribePlaceLabel(const SessionState& state, const std::string& place_id)
{
    LocationId location_id = kLocationUnknown;
    if (ParseCanonicalPlaceId(place_id, &location_id)) {
        return LocationIdToString(location_id);
    }

    if (IsGeneratedPlaceId(place_id)) {
        for (size_t index = 0; index < state.generated_rooms.size(); ++index) {
            if (state.generated_rooms[index].room_id == place_id) {
                return BuildPlaceLabelFromSpatialState(state.generated_rooms[index].spatial_state);
            }
        }
        return place_id;
    }

    if (!place_id.empty()) {
        return place_id;
    }

    return BuildPlaceLabelFromSpatialState(state.spatial_state);
}

std::string DescribeCurrentPlaceLabel(const SessionState& state)
{
    return DescribePlaceLabel(state, state.current_place_id);
}

bool GetCanonicalWorldPose(LocationId location_id, float* world_x, float* world_z)
{
    if (!world_x || !world_z) {
        return false;
    }

    switch (location_id) {
    case kLocationGate:
        *world_x = 0.0f;
        *world_z = -6.0f;
        return true;
    case kLocationServerAisles:
        *world_x = 0.0f;
        *world_z = -1.5f;
        return true;
    case kLocationRoofWatch:
        *world_x = 0.0f;
        *world_z = 0.0f;
        return true;
    default:
        break;
    }

    return false;
}

void SetSpatialWorldPose(SpatialState* state, float world_x, float world_z)
{
    if (!state) {
        return;
    }

    state->world_pose_known = true;
    state->world_x = world_x;
    state->world_z = world_z;
}

bool ResolvePlaceWorldPose(const SessionState& state, const std::string& place_id, float* world_x, float* world_z)
{
    if (!world_x || !world_z || place_id.empty()) {
        return false;
    }

    if (!state.current_place_id.empty() && place_id == state.current_place_id && state.spatial_state.world_pose_known) {
        *world_x = state.spatial_state.world_x;
        *world_z = state.spatial_state.world_z;
        return true;
    }

    LocationId location_id = kLocationUnknown;
    if (ParseCanonicalPlaceId(place_id, &location_id)) {
        return GetCanonicalWorldPose(location_id, world_x, world_z);
    }

    for (size_t index = 0; index < state.generated_rooms.size(); ++index) {
        if (state.generated_rooms[index].room_id == place_id && state.generated_rooms[index].spatial_state.world_pose_known) {
            *world_x = state.generated_rooms[index].spatial_state.world_x;
            *world_z = state.generated_rooms[index].spatial_state.world_z;
            return true;
        }
    }

    return false;
}

bool AssignWorldPoseFromTraversal(
    const SessionState& state,
    const std::string& from_place_id,
    CardinalDirection direction,
    SpatialState* spatial_state)
{
    if (!spatial_state || direction == kDirectionUnknown) {
        return false;
    }

    float world_x = 0.0f;
    float world_z = 0.0f;
    if (!ResolvePlaceWorldPose(state, from_place_id, &world_x, &world_z)) {
        return false;
    }

    ApplyTraversalStep(direction, &world_x, &world_z);
    SetSpatialWorldPose(spatial_state, world_x, world_z);
    return true;
}

float ComputeSpatialWorldRadius(const SpatialState& state)
{
    if (!state.world_pose_known) {
        return -1.0f;
    }
    return sqrtf(state.world_x * state.world_x + state.world_z * state.world_z);
}

float ComputeSpatialWorldAngleDegrees(const SpatialState& state)
{
    if (!state.world_pose_known) {
        return 0.0f;
    }
    return atan2f(state.world_x, -state.world_z) * kPiDegrees;
}

const char* DescribeSpatialWorldBand(const SpatialState& state)
{
    switch (ClassifyHiddenWorldBandFromRadius(ComputeSpatialWorldRadius(state))) {
    case kHiddenWorldBandCentralCore:
        return "central core";
    case kHiddenWorldBandInnerTechnicalRing:
        return "inner technical ring";
    case kHiddenWorldBandPerimeterSeam:
        return "perimeter seam";
    case kHiddenWorldBandOuterParapet:
        return "outer parapet";
    case kHiddenWorldBandOpenDesert:
        return "open desert";
    default:
        return "unknown";
    }
}

const char* DescribeSpatialGateRelation(const SpatialState& state)
{
    if (!state.world_pose_known) {
        return "unknown";
    }

    const float abs_x = fabsf(state.world_x);
    if (state.world_z <= -abs_x * 0.75f) {
        return "entry-facing side";
    }
    if (state.world_z >= abs_x * 0.75f) {
        return "far side opposite the entry gate";
    }
    return state.world_x >= 0.0f ? "east flank" : "west flank";
}

const char* DescribeSpatialSkyExposure(const SpatialState& state)
{
    switch (ClassifyHiddenWorldBandFromRadius(ComputeSpatialWorldRadius(state))) {
    case kHiddenWorldBandCentralCore:
    case kHiddenWorldBandInnerTechnicalRing:
        return "sealed interior likely";
    case kHiddenWorldBandPerimeterSeam:
        return "partial openings plausible";
    case kHiddenWorldBandOuterParapet:
        return "open sky likely";
    case kHiddenWorldBandOpenDesert:
        return "fully open desert";
    default:
        return "unknown";
    }
}

HardState MakeInitialHardState()
{
    HardState state;
    state.turn_number = 1;
    state.move_count = 0;
    state.score = 0;
    state.current_location_id = kLocationGate;
    state.alert_level = 1;
    state.datacenter_temperature_c = kDefaultDatacenterTemperatureC;
    state.cooling_state = kResourceStable;
    state.water_state = kResourceStable;
    state.power_state = kResourceStable;
    state.named_entities.push_back("duty_officer");
    state.unresolved_threats.push_back("possible_external_attack");
    return state;
}

SoftState MakeInitialSoftState()
{
    SoftState state;
    state.rolling_summary =
        "An officer has just arrived at an isolated datacenter built at the edge of a desert.";
    state.atmosphere = "administrative, dry, watchful";
    state.active_hypotheses.push_back("the threat may be real");
    state.active_hypotheses.push_back("the institution may be manufacturing the threat");
    return state;
}

void NormalizeSessionState(SessionState* state)
{
    if (!state) {
        return;
    }

    if (state->next_generated_room_index <= 0) {
        state->next_generated_room_index = 1;
    }

    if (state->current_place_id.empty()) {
        if (state->spatial_state.location_id != kLocationUnknown) {
            state->current_place_id = BuildCanonicalPlaceId(state->spatial_state.location_id);
        } else if (state->hard_state.current_location_id != kLocationUnknown) {
            state->current_place_id = BuildCanonicalPlaceId(state->hard_state.current_location_id);
        }
    }

    if (!state->current_place_id.empty() && !IsKnownPlaceIdInSession(*state, state->current_place_id)) {
        state->current_place_id.clear();
    }
    if (!state->origin_place_id.empty() && !IsKnownPlaceIdInSession(*state, state->origin_place_id)) {
        state->origin_place_id.clear();
    }
    if (state->origin_place_id.empty()) {
        if (!state->current_place_id.empty()) {
            state->origin_place_id = state->current_place_id;
        } else if (state->spatial_state.location_id != kLocationUnknown) {
            state->origin_place_id = BuildCanonicalPlaceId(state->spatial_state.location_id);
        } else if (state->hard_state.current_location_id != kLocationUnknown) {
            state->origin_place_id = BuildCanonicalPlaceId(state->hard_state.current_location_id);
        }
    }

    const bool in_generated_room = IsGeneratedPlaceId(state->current_place_id);

    if (state->hard_state.turn_number <= 0) {
        state->hard_state.turn_number = 1;
    }
    if (state->hard_state.move_count < 0) {
        state->hard_state.move_count = 0;
    }
    if (state->hard_state.score < 0) {
        state->hard_state.score = 0;
    }
    state->hard_state.datacenter_temperature_c =
        ClampDatacenterTemperatureC(state->hard_state.datacenter_temperature_c);

    if (in_generated_room) {
        state->hard_state.current_location_id = kLocationUnknown;
        state->spatial_state.location_id = kLocationUnknown;
    } else if (state->hard_state.current_location_id == kLocationUnknown && state->spatial_state.location_id != kLocationUnknown) {
        state->hard_state.current_location_id = state->spatial_state.location_id;
    }
    if (!in_generated_room && state->spatial_state.location_id == kLocationUnknown && state->hard_state.current_location_id != kLocationUnknown) {
        state->spatial_state.location_id = state->hard_state.current_location_id;
    }
    if (!in_generated_room && state->hard_state.current_location_id == kLocationUnknown) {
        state->hard_state.current_location_id = kLocationGate;
    }
    if (!in_generated_room && state->spatial_state.location_id == kLocationUnknown) {
        state->spatial_state.location_id = state->hard_state.current_location_id;
    }
    if (state->hard_state.alert_level <= 0 && state->spatial_state.alert_level > 0) {
        state->hard_state.alert_level = state->spatial_state.alert_level;
    }
    if (state->spatial_state.alert_level <= 0 && state->hard_state.alert_level > 0) {
        state->spatial_state.alert_level = state->hard_state.alert_level;
    }
    if (state->hard_state.alert_level <= 0) {
        state->hard_state.alert_level = 1;
    }
    if (state->spatial_state.alert_level <= 0) {
        state->spatial_state.alert_level = state->hard_state.alert_level;
    }

    if (state->spatial_state.room_title.empty()) {
        state->spatial_state.room_title = BuildPlaceLabelFromSpatialState(state->spatial_state);
    }

    if (!in_generated_room && !state->spatial_state.world_pose_known) {
        float world_x = 0.0f;
        float world_z = 0.0f;
        if (GetCanonicalWorldPose(state->spatial_state.location_id, &world_x, &world_z)) {
            SetSpatialWorldPose(&state->spatial_state, world_x, world_z);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t index = 0; index < state->generated_rooms.size(); ++index) {
            SpatialState* room_state = &state->generated_rooms[index].spatial_state;
            if (room_state->world_pose_known) {
                continue;
            }

            float world_x = 0.0f;
            float world_z = 0.0f;
            if (TryInferWorldPoseFromLinks(*state, state->generated_rooms[index].room_id, &world_x, &world_z)) {
                SetSpatialWorldPose(room_state, world_x, world_z);
                changed = true;
            }
        }
    }

    if (in_generated_room && !state->spatial_state.world_pose_known) {
        float world_x = 0.0f;
        float world_z = 0.0f;
        if (ResolvePlaceWorldPose(*state, state->current_place_id, &world_x, &world_z)) {
            SetSpatialWorldPose(&state->spatial_state, world_x, world_z);
        }
    }

    for (size_t index = 0; index < state->history.size(); ++index) {
        if (state->history[index].location_label.empty()) {
            if (state->history[index].location_id != kLocationUnknown) {
                state->history[index].location_label = LocationIdToString(state->history[index].location_id);
            } else {
                state->history[index].location_label = "unknown";
            }
        }
    }
}

bool SerializeSessionStateToJsonString(const SessionState& state, std::string* json_text)
{
    if (!json_text) {
        return false;
    }

    json root = json::object();
    root["hard_state"] = MakeHardStateJson(state.hard_state);
    root["soft_state"] = MakeSoftStateJson(state.soft_state);
    root["spatial_state"] = MakeSpatialStateJson(state.spatial_state);
    root["history"] = MakeSessionHistoryJson(state.history);
    root["origin_place_id"] = state.origin_place_id;
    root["current_place_id"] = state.current_place_id;
    root["next_generated_room_index"] = state.next_generated_room_index;
    root["generated_rooms"] = MakeGeneratedRoomsJson(state.generated_rooms);
    root["room_links"] = MakeRoomLinksJson(state.room_links);
    *json_text = root.dump(2);
    return true;
}

bool ParseSessionStateFromJson(
    const char* json_text,
    SessionState* state,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!json_text || !state) {
        SetError(error_buffer, error_buffer_size, "Invalid session JSON: %s", "(null)");
        return false;
    }

    *state = SessionState();
    try {
        const json root = json::parse(json_text);
        if (!root.is_object()) {
            SetError(error_buffer, error_buffer_size, "Session JSON is not an object: %s", "(root)");
            return false;
        }

        if (root.contains("hard_state")) {
            ParseHardStateNode(root["hard_state"], &state->hard_state);
        }
        if (root.contains("soft_state")) {
            ParseSoftStateNode(root["soft_state"], &state->soft_state);
        }
        if (root.contains("spatial_state")) {
            ParseSpatialStateNode(root["spatial_state"], &state->spatial_state);
        }
        if (root.contains("history")) {
            ParseHistoryNode(root["history"], &state->history);
        }
        state->origin_place_id = ReadStringNode(root, "origin_place_id");
        state->current_place_id = ReadStringNode(root, "current_place_id");
        state->next_generated_room_index = ReadIntNode(root, "next_generated_room_index", state->next_generated_room_index);
        if (root.contains("generated_rooms")) {
            ParseGeneratedRoomsNode(root["generated_rooms"], &state->generated_rooms);
        }
        if (root.contains("room_links")) {
            ParseRoomLinksNode(root["room_links"], &state->room_links);
        }

        NormalizeSessionState(state);
        return true;
    } catch (const std::exception& exception) {
        SetError(error_buffer, error_buffer_size, "Failed to parse session JSON: %s", exception.what());
        return false;
    }
}

void PrintHardStateSummary(const HardState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "HardState\n");
    fprintf(out, "  turn_number: %d\n", state.turn_number);
    fprintf(out, "  move_count: %d\n", state.move_count);
    fprintf(out, "  score: %d\n", state.score);
    fprintf(out, "  current_location_id: %s\n", LocationIdToString(state.current_location_id));
    fprintf(out, "  alert_level: %d\n", state.alert_level);
    fprintf(out, "  datacenter_temperature_c: %d\n", state.datacenter_temperature_c);
    fprintf(out, "  cooling_state: %s\n", ResourceStateToString(state.cooling_state));
    fprintf(out, "  water_state: %s\n", ResourceStateToString(state.water_state));
    fprintf(out, "  power_state: %s\n", ResourceStateToString(state.power_state));
    PrintStringList("  inventory_items", state.inventory_items, out);
    PrintStringList("  named_entities", state.named_entities, out);
    PrintStringList("  unresolved_threats", state.unresolved_threats, out);
}

void PrintSoftStateSummary(const SoftState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "SoftState\n");
    fprintf(out, "  rolling_summary: %s\n", state.rolling_summary.empty() ? "(empty)" : state.rolling_summary.c_str());
    fprintf(out, "  atmosphere: %s\n", state.atmosphere.empty() ? "(empty)" : state.atmosphere.c_str());
    PrintStringList("  active_hypotheses", state.active_hypotheses, out);
    PrintStringList("  tolerated_incoherences", state.tolerated_incoherences, out);
}

void PrintSpatialStateSummary(const SpatialState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "SpatialState\n");
    fprintf(out, "  location_id: %s\n", LocationIdToString(state.location_id));
    fprintf(out, "  room_title: %s\n", state.room_title.empty() ? "(empty)" : state.room_title.c_str());
    fprintf(out, "  room_summary: %s\n", state.room_summary.empty() ? "(empty)" : state.room_summary.c_str());
    fprintf(out, "  location_archetype: %s\n", state.location_archetype.empty() ? "(empty)" : state.location_archetype.c_str());
    fprintf(out, "  canonical_fixture: %s\n", state.canonical_fixture.empty() ? "(empty)" : state.canonical_fixture.c_str());
    if (state.world_pose_known) {
        fprintf(
            out,
            "  world_pose: (%.2f, %.2f) radius=%.2f angle=%.1f band=%s gate_relation=%s sky=%s\n",
            state.world_x,
            state.world_z,
            ComputeSpatialWorldRadius(state),
            ComputeSpatialWorldAngleDegrees(state),
            DescribeSpatialWorldBand(state),
            DescribeSpatialGateRelation(state),
            DescribeSpatialSkyExposure(state));
    } else {
        fprintf(out, "  world_pose: (unknown)\n");
    }
    fprintf(out, "  time_of_day: %s\n", TimeOfDayToString(state.time_of_day));
    fprintf(out, "  visibility_level: %s\n", VisibilityLevelToString(state.visibility_level));
    fprintf(out, "  desert_state: %s\n", DesertStateToString(state.desert_state));
    fprintf(out, "  interior_density: %s\n", InteriorDensityToString(state.interior_density));
    fprintf(out, "  alert_level: %d\n", state.alert_level);
    PrintStringList("  anchors", state.anchors, out);
    PrintStringList("  visible_objects", state.visible_objects, out);
    PrintStringList("  blocked_exits", state.blocked_exits, out);
    PrintStringList("  spatial_anomalies", state.spatial_anomalies, out);
    PrintStringList("  scene_constraints", state.scene_constraints, out);
}

void PrintSessionStateSummary(const SessionState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "SessionState\n");
    fprintf(out, "  origin_place_id: %s\n", state.origin_place_id.empty() ? "(empty)" : state.origin_place_id.c_str());
    fprintf(out, "  current_place_id: %s\n", state.current_place_id.empty() ? "(empty)" : state.current_place_id.c_str());
    fprintf(out, "  current_place_label: %s\n", DescribeCurrentPlaceLabel(state).c_str());
    fprintf(out, "  generated_rooms: %zu\n", state.generated_rooms.size());
    fprintf(out, "  room_links: %zu\n", state.room_links.size());
    PrintHardStateSummary(state.hard_state, out);
    PrintSoftStateSummary(state.soft_state, out);
    PrintSpatialStateSummary(state.spatial_state, out);
    fprintf(out, "  history_entries: %zu\n", state.history.size());
}

void PrintSessionHistory(const SessionState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "SessionHistory\n");
    if (state.history.empty()) {
        fprintf(out, "  (empty)\n");
        return;
    }

    for (size_t index = 0; index < state.history.size(); ++index) {
        const SessionTurnRecord& record = state.history[index];
        fprintf(
            out,
            "  turn %d @ %s\n",
            record.turn_number,
            record.location_label.empty() ? LocationIdToString(record.location_id) : record.location_label.c_str());
        fprintf(out, "    command: %s\n", record.player_command.empty() ? "(empty)" : record.player_command.c_str());
        fprintf(out, "    intent: %s\n", record.intent.empty() ? "(empty)" : record.intent.c_str());
        fprintf(out, "    narration: %s\n", record.narration.empty() ? "(empty)" : record.narration.c_str());
        if (!record.clarification.empty()) {
            fprintf(out, "    clarification: %s\n", record.clarification.c_str());
        }
    }
}

int ComputeRoomGraphDistance(const SessionState& state, const std::string& from_place_id, const std::string& to_place_id)
{
    if (from_place_id.empty() || to_place_id.empty()) {
        return -1;
    }
    if (from_place_id == to_place_id) {
        return 0;
    }

    std::vector<std::string> visited;
    std::vector<std::string> frontier;
    visited.push_back(from_place_id);
    frontier.push_back(from_place_id);

    int distance = 0;
    while (!frontier.empty()) {
        ++distance;
        std::vector<std::string> next_frontier;
        for (size_t frontier_index = 0; frontier_index < frontier.size(); ++frontier_index) {
            const std::string& current_place_id = frontier[frontier_index];
            for (size_t link_index = 0; link_index < state.room_links.size(); ++link_index) {
                const RoomLink& link = state.room_links[link_index];
                if (link.from_place_id != current_place_id || link.to_place_id.empty()) {
                    continue;
                }
                if (link.to_place_id == to_place_id) {
                    return distance;
                }
                if (!VectorContainsString(visited, link.to_place_id)) {
                    visited.push_back(link.to_place_id);
                    next_frontier.push_back(link.to_place_id);
                }
            }
        }
        frontier.swap(next_frontier);
    }

    return -1;
}

int ComputeDistanceFromOriginPlace(const SessionState& state, const std::string& place_id)
{
    if (place_id.empty()) {
        return -1;
    }

    const std::string origin_place_id = state.origin_place_id.empty() ? state.current_place_id : state.origin_place_id;
    if (origin_place_id.empty()) {
        return -1;
    }

    return ComputeRoomGraphDistance(state, origin_place_id, place_id);
}

}  // namespace liminal
