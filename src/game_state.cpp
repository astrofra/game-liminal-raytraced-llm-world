#include "game_state.h"

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

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument ? argument : "(null)");
}

static json MakeHardStateJson(const HardState& state)
{
    json node = json::object();
    node["turn_number"] = state.turn_number;
    node["current_location_id"] = LocationIdToString(state.current_location_id);
    node["alert_level"] = state.alert_level;
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
    node["location_archetype"] = state.location_archetype;
    node["canonical_fixture"] = state.canonical_fixture;
    node["time_of_day"] = TimeOfDayToString(state.time_of_day);
    node["visibility_level"] = VisibilityLevelToString(state.visibility_level);
    node["desert_state"] = DesertStateToString(state.desert_state);
    node["interior_density"] = InteriorDensityToString(state.interior_density);
    node["alert_level"] = state.alert_level;
    node["anchors"] = state.anchors;
    node["visible_objects"] = state.visible_objects;
    node["blocked_exits"] = state.blocked_exits;
    node["spatial_anomalies"] = state.spatial_anomalies;
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
        item["player_command"] = record.player_command;
        item["intent"] = record.intent;
        item["narration"] = record.narration;
        item["clarification"] = record.clarification;
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
    ParseLocationId(ReadStringNode(node, "current_location_id").c_str(), &state->current_location_id);
    state->alert_level = ReadIntNode(node, "alert_level", state->alert_level);
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
    state->location_archetype = ReadStringNode(node, "location_archetype");
    state->canonical_fixture = ReadStringNode(node, "canonical_fixture");
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
        record.player_command = ReadStringNode(node[index], "player_command");
        record.intent = ReadStringNode(node[index], "intent");
        record.narration = ReadStringNode(node[index], "narration");
        record.clarification = ReadStringNode(node[index], "clarification");
        history->push_back(record);
    }
}

}  // namespace

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

HardState MakeInitialHardState()
{
    HardState state;
    state.turn_number = 1;
    state.current_location_id = kLocationGate;
    state.alert_level = 1;
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

    if (state->hard_state.turn_number <= 0) {
        state->hard_state.turn_number = 1;
    }
    if (state->hard_state.current_location_id == kLocationUnknown && state->spatial_state.location_id != kLocationUnknown) {
        state->hard_state.current_location_id = state->spatial_state.location_id;
    }
    if (state->spatial_state.location_id == kLocationUnknown && state->hard_state.current_location_id != kLocationUnknown) {
        state->spatial_state.location_id = state->hard_state.current_location_id;
    }
    if (state->hard_state.current_location_id == kLocationUnknown) {
        state->hard_state.current_location_id = kLocationGate;
    }
    if (state->spatial_state.location_id == kLocationUnknown) {
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
    fprintf(out, "  current_location_id: %s\n", LocationIdToString(state.current_location_id));
    fprintf(out, "  alert_level: %d\n", state.alert_level);
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
    fprintf(out, "  location_archetype: %s\n", state.location_archetype.empty() ? "(empty)" : state.location_archetype.c_str());
    fprintf(out, "  canonical_fixture: %s\n", state.canonical_fixture.empty() ? "(empty)" : state.canonical_fixture.c_str());
    fprintf(out, "  time_of_day: %s\n", TimeOfDayToString(state.time_of_day));
    fprintf(out, "  visibility_level: %s\n", VisibilityLevelToString(state.visibility_level));
    fprintf(out, "  desert_state: %s\n", DesertStateToString(state.desert_state));
    fprintf(out, "  interior_density: %s\n", InteriorDensityToString(state.interior_density));
    fprintf(out, "  alert_level: %d\n", state.alert_level);
    PrintStringList("  anchors", state.anchors, out);
    PrintStringList("  visible_objects", state.visible_objects, out);
    PrintStringList("  blocked_exits", state.blocked_exits, out);
    PrintStringList("  spatial_anomalies", state.spatial_anomalies, out);
}

void PrintSessionStateSummary(const SessionState& state, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "SessionState\n");
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
        fprintf(out, "  turn %d @ %s\n", record.turn_number, LocationIdToString(record.location_id));
        fprintf(out, "    command: %s\n", record.player_command.empty() ? "(empty)" : record.player_command.c_str());
        fprintf(out, "    intent: %s\n", record.intent.empty() ? "(empty)" : record.intent.c_str());
        fprintf(out, "    narration: %s\n", record.narration.empty() ? "(empty)" : record.narration.c_str());
        if (!record.clarification.empty()) {
            fprintf(out, "    clarification: %s\n", record.clarification.c_str());
        }
    }
}

}  // namespace liminal
