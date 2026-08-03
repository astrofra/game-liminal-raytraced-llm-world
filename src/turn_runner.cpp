#include "turn_runner.h"

#include <stdio.h>
#include <string.h>

#include "scene_compiler.h"
#include "turn_contract.h"
#include "nlohmann/json.hpp"

namespace liminal {

namespace {

using json = nlohmann::json;

struct StreamForwarder {
    HeadlessTurnStreamCallback callback;
    HeadlessTurnStreamPhase phase;
    void* user_data;

    StreamForwarder()
        : callback(0)
        , phase(kHeadlessTurnStreamPrimaryResponse)
        , user_data(0)
    {
    }
};

struct GeneratedRoomDraft {
    std::string title;
    std::string summary;
    std::string arrival_narration;
    int move_cost;
    int score_delta;
    bool temperature_changed;
    int next_datacenter_temperature_c;
    SpatialState spatial_state;

    GeneratedRoomDraft()
        : move_cost(1)
        , score_delta(0)
        , temperature_changed(false)
        , next_datacenter_temperature_c(kDefaultDatacenterTemperatureC)
    {
    }
};

static void FinalizeGeneratedRoomDraft(GeneratedRoomDraft* draft, CardinalDirection direction);

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument ? argument : "(null)");
}

static void AddUniqueString(std::vector<std::string>* values, const std::string& value)
{
    if (!values || value.empty()) {
        return;
    }

    for (size_t index = 0; index < values->size(); ++index) {
        if ((*values)[index] == value) {
            return;
        }
    }
    values->push_back(value);
}

static void RemoveString(std::vector<std::string>* values, const std::string& value)
{
    if (!values || value.empty()) {
        return;
    }

    for (size_t index = 0; index < values->size(); ++index) {
        if ((*values)[index] == value) {
            values->erase(values->begin() + index);
            return;
        }
    }
}

static std::vector<std::string> ReadStringArray(const json& node)
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

static std::string ReadStringValue(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_string()) {
        return std::string();
    }
    return object[key].get<std::string>();
}

static std::string TrimWhitespaceCopy(const std::string& text)
{
    size_t start = 0;
    while (start < text.size()) {
        const char value = text[start];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        ++start;
    }

    size_t end = text.size();
    while (end > start) {
        const char value = text[end - 1];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        --end;
    }

    return text.substr(start, end - start);
}

static std::string NormalizeCodeBlockText(const std::string& text)
{
    const std::string trimmed = TrimWhitespaceCopy(text);
    if (trimmed.size() < 6 || trimmed.compare(0, 3, "```") != 0) {
        return trimmed;
    }

    const size_t first_newline = trimmed.find('\n');
    if (first_newline == std::string::npos) {
        return trimmed;
    }

    const size_t closing_fence = trimmed.rfind("```");
    if (closing_fence == std::string::npos || closing_fence <= first_newline) {
        return trimmed;
    }

    return TrimWhitespaceCopy(trimmed.substr(first_newline + 1, closing_fence - first_newline - 1));
}

static std::string CollapseWhitespaceCopy(const std::string& text)
{
    std::string output;
    output.reserve(text.size());

    bool previous_was_space = false;
    for (size_t index = 0; index < text.size(); ++index) {
        const char value = text[index];
        const bool is_space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
        if (is_space) {
            if (!previous_was_space && !output.empty()) {
                output.push_back(' ');
            }
            previous_was_space = true;
            continue;
        }

        output.push_back(value);
        previous_was_space = false;
    }

    return TrimWhitespaceCopy(output);
}

static std::string ConstrainNarrationText(const std::string& text, size_t max_chars, int max_sentences)
{
    std::string constrained = CollapseWhitespaceCopy(text);
    if (constrained.empty()) {
        return constrained;
    }

    if (max_sentences > 0) {
        int sentence_count = 0;
        for (size_t index = 0; index < constrained.size(); ++index) {
            const char value = constrained[index];
            if (value == '.' || value == '!' || value == '?') {
                ++sentence_count;
                if (sentence_count >= max_sentences) {
                    constrained = TrimWhitespaceCopy(constrained.substr(0, index + 1));
                    break;
                }
            }
        }
    }

    if (constrained.size() > max_chars) {
        size_t split = constrained.rfind(' ', max_chars);
        if (split == std::string::npos || split < max_chars / 2) {
            split = max_chars;
        }
        constrained = TrimWhitespaceCopy(constrained.substr(0, split));
        if (!constrained.empty()) {
            constrained.append("...");
        }
    }

    return constrained;
}

static void RemoveBlockedDirection(std::vector<std::string>* blocked_exits, CardinalDirection direction)
{
    if (!blocked_exits || direction == kDirectionUnknown) {
        return;
    }

    for (size_t index = 0; index < blocked_exits->size();) {
        CardinalDirection blocked_direction = kDirectionUnknown;
        if (ParseCardinalDirection((*blocked_exits)[index].c_str(), &blocked_direction) && blocked_direction == direction) {
            blocked_exits->erase(blocked_exits->begin() + index);
            continue;
        }
        ++index;
    }
}

static bool ExtractQuotedJsonField(const std::string& text, const char* key, std::string* value)
{
    if (!key || !value) {
        return false;
    }

    const std::string pattern = std::string("\"") + key + "\"";
    const size_t key_pos = text.find(pattern);
    if (key_pos == std::string::npos) {
        return false;
    }

    const size_t colon_pos = text.find(':', key_pos + pattern.size());
    if (colon_pos == std::string::npos) {
        return false;
    }

    size_t quote_pos = text.find('"', colon_pos + 1);
    if (quote_pos == std::string::npos) {
        return false;
    }
    ++quote_pos;

    std::string output;
    bool escaping = false;
    for (size_t index = quote_pos; index < text.size(); ++index) {
        const char current = text[index];
        if (escaping) {
            if (current == 'n' || current == 'r' || current == 't') {
                output.push_back(' ');
            } else {
                output.push_back(current);
            }
            escaping = false;
            continue;
        }
        if (current == '\\') {
            escaping = true;
            continue;
        }
        if (current == '"') {
            *value = output;
            return true;
        }
        output.push_back(current);
    }

    if (!output.empty()) {
        *value = output;
        return true;
    }

    return false;
}

static std::string BuildFallbackNarration(const std::string& raw_response_text)
{
    std::string extracted_narration;
    if (ExtractQuotedJsonField(raw_response_text, "narration", &extracted_narration)) {
        const std::string collapsed = CollapseWhitespaceCopy(extracted_narration);
        if (!collapsed.empty()) {
            return collapsed;
        }
    }

    std::string text = CollapseWhitespaceCopy(NormalizeCodeBlockText(raw_response_text));
    if (text.empty()) {
        return "The system hesitates for a moment. The place remains unchanged.";
    }

    if (text.size() > 280) {
        text.resize(280);
        text.append("...");
    }

    if (text == "`" || text == "```" || text == "json") {
        return "The system hesitates for a moment. The place remains unchanged.";
    }

    return ConstrainNarrationText(text, 280, 3);
}

static TurnResult MakeFallbackTurnResult(const std::string& raw_response_text)
{
    TurnResult turn_result;
    turn_result.intent = "fallback_noop";
    ExtractQuotedJsonField(raw_response_text, "intent", &turn_result.intent);
    turn_result.narration = BuildFallbackNarration(raw_response_text);
    turn_result.clarification = "The world state was kept unchanged after a malformed model response.";
    turn_result.continuity_notes.push_back("Malformed model response was ignored; hard state and spatial state were kept stable.");
    return turn_result;
}

static bool ExtractFirstJsonObject(const char* text, std::string* object_text)
{
    if (!text || !object_text) {
        return false;
    }

    const char* start = 0;
    while (*text) {
        if (*text == '{') {
            start = text;
            break;
        }
        ++text;
    }
    if (!start) {
        return false;
    }

    bool in_string = false;
    bool escaping = false;
    int depth = 0;
    for (const char* cursor = start; *cursor; ++cursor) {
        const char current = *cursor;
        if (escaping) {
            escaping = false;
            continue;
        }
        if (current == '\\' && in_string) {
            escaping = true;
            continue;
        }
        if (current == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (current == '{') {
            ++depth;
        } else if (current == '}') {
            --depth;
            if (depth == 0) {
                object_text->assign(start, static_cast<size_t>(cursor - start + 1));
                return true;
            }
        }
    }

    return false;
}

static bool ForwardStreamChunk(const char* accumulated_text, const char* delta_text, void* user_data)
{
    StreamForwarder* forwarder = static_cast<StreamForwarder*>(user_data);
    if (!forwarder || !forwarder->callback) {
        return true;
    }

    return forwarder->callback(
        forwarder->phase,
        accumulated_text ? accumulated_text : "",
        delta_text ? delta_text : "",
        forwarder->user_data);
}

static bool RepairTurnResultJson(
    const HeadlessTurnConfig& config,
    const std::string& raw_response_text,
    TurnResult* turn_result,
    std::string* repair_response_text,
    char* error_buffer,
    size_t error_buffer_size)
{
    std::vector<LlmPromptMessage> repair_messages;
    repair_messages.push_back(LlmPromptMessage());
    repair_messages.back().role = "system";
    repair_messages.back().content =
        "You repair malformed structured interactive-fiction turn outputs. "
        "Return valid JSON only. Do not use markdown fences. "
        "If a field is missing, keep it empty or use no-op deltas.";
    repair_messages.push_back(LlmPromptMessage());
    repair_messages.back().role = "user";
    repair_messages.back().content =
        std::string("Rewrite the following malformed output into exactly one JSON object matching this schema.\n\n") +
        BuildTurnResultSchemaText() +
        "\n\nMalformed output to repair\n" +
        raw_response_text;

    LlmGenerationConfig repair_config = config.generation_config;
    repair_config.use_json_grammar = false;

    LlmGenerationResult repair_generation_result;
    StreamForwarder stream_forwarder;
    stream_forwarder.callback = config.stream_callback;
    stream_forwarder.phase = kHeadlessTurnStreamRepairResponse;
    stream_forwarder.user_data = config.stream_user_data;
    if (!GenerateChatCompletion(
            repair_config,
            repair_messages,
            config.stream_callback ? ForwardStreamChunk : 0,
            config.stream_callback ? &stream_forwarder : 0,
            &repair_generation_result)) {
        SetError(
            error_buffer,
            error_buffer_size,
            "LLM JSON repair failed: %s",
            repair_generation_result.error_message.empty() ? "unknown error" : repair_generation_result.error_message.c_str());
        return false;
    }

    if (repair_response_text) {
        *repair_response_text = repair_generation_result.response_text;
    }

    if (!ParseTurnResultJson(repair_generation_result.response_text.c_str(), turn_result, error_buffer, error_buffer_size)) {
        return false;
    }

    return true;
}

static bool ReadBoolValue(const json& object, const char* key, bool default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_boolean()) {
        return default_value;
    }
    return object[key].get<bool>();
}

static int ReadIntValue(const json& object, const char* key, int default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_number_integer()) {
        return default_value;
    }
    return object[key].get<int>();
}

static void ParseHardStateDeltaNode(const json& node, HardStateDelta* delta)
{
    if (!delta || !node.is_object()) {
        return;
    }

    delta->location_changed = ReadBoolValue(node, "location_changed", false);
    ParseLocationId(ReadStringValue(node, "next_location_id").c_str(), &delta->next_location_id);
    delta->move_count_changed = ReadBoolValue(node, "move_count_changed", false);
    delta->next_move_count = ReadIntValue(node, "next_move_count", 0);
    delta->score_changed = ReadBoolValue(node, "score_changed", false);
    delta->next_score = ReadIntValue(node, "next_score", 0);
    delta->alert_level_changed = ReadBoolValue(node, "alert_level_changed", false);
    delta->next_alert_level = ReadIntValue(node, "next_alert_level", 0);
    const bool has_next_temperature =
        node.contains("next_datacenter_temperature_c") &&
        node["next_datacenter_temperature_c"].is_number_integer();
    delta->temperature_changed = ReadBoolValue(node, "temperature_changed", false) && has_next_temperature;
    delta->next_datacenter_temperature_c =
        ClampDatacenterTemperatureC(
            ReadIntValue(node, "next_datacenter_temperature_c", delta->next_datacenter_temperature_c));
    delta->cooling_state_changed = ReadBoolValue(node, "cooling_state_changed", false);
    ParseResourceState(ReadStringValue(node, "next_cooling_state").c_str(), &delta->next_cooling_state);
    delta->water_state_changed = ReadBoolValue(node, "water_state_changed", false);
    ParseResourceState(ReadStringValue(node, "next_water_state").c_str(), &delta->next_water_state);
    delta->power_state_changed = ReadBoolValue(node, "power_state_changed", false);
    ParseResourceState(ReadStringValue(node, "next_power_state").c_str(), &delta->next_power_state);
    delta->inventory_add = ReadStringArray(node.contains("inventory_add") ? node["inventory_add"] : json());
    delta->inventory_remove = ReadStringArray(node.contains("inventory_remove") ? node["inventory_remove"] : json());
    delta->threats_add = ReadStringArray(node.contains("threats_add") ? node["threats_add"] : json());
    delta->threats_remove = ReadStringArray(node.contains("threats_remove") ? node["threats_remove"] : json());
}

static void ParseSpatialStateDeltaNode(const json& node, SpatialStateDelta* delta)
{
    if (!delta || !node.is_object()) {
        return;
    }

    delta->location_changed = ReadBoolValue(node, "location_changed", false);
    ParseLocationId(ReadStringValue(node, "next_location_id").c_str(), &delta->next_location_id);
    delta->time_of_day_changed = ReadBoolValue(node, "time_of_day_changed", false);
    ParseTimeOfDay(ReadStringValue(node, "next_time_of_day").c_str(), &delta->next_time_of_day);
    delta->visibility_changed = ReadBoolValue(node, "visibility_changed", false);
    ParseVisibilityLevel(ReadStringValue(node, "next_visibility_level").c_str(), &delta->next_visibility_level);
    delta->desert_state_changed = ReadBoolValue(node, "desert_state_changed", false);
    ParseDesertState(ReadStringValue(node, "next_desert_state").c_str(), &delta->next_desert_state);
    delta->interior_density_changed = ReadBoolValue(node, "interior_density_changed", false);
    ParseInteriorDensity(ReadStringValue(node, "next_interior_density").c_str(), &delta->next_interior_density);
    delta->alert_level_changed = ReadBoolValue(node, "alert_level_changed", false);
    delta->next_alert_level = ReadIntValue(node, "next_alert_level", 0);
    delta->anchors_present_changed = node.contains("anchors_present");
    delta->visible_objects_changed = node.contains("visible_objects");
    delta->blocked_exits_changed = node.contains("blocked_exits");
    delta->spatial_anomalies_changed = node.contains("spatial_anomalies");
    delta->scene_constraints_changed = node.contains("scene_constraints");
    delta->anchors_present = ReadStringArray(node.contains("anchors_present") ? node["anchors_present"] : json());
    delta->visible_objects = ReadStringArray(node.contains("visible_objects") ? node["visible_objects"] : json());
    delta->blocked_exits = ReadStringArray(node.contains("blocked_exits") ? node["blocked_exits"] : json());
    delta->spatial_anomalies = ReadStringArray(node.contains("spatial_anomalies") ? node["spatial_anomalies"] : json());
    delta->scene_constraints = ReadStringArray(node.contains("scene_constraints") ? node["scene_constraints"] : json());
}

static void ApplyHardDelta(HardState* state, const HardStateDelta& delta)
{
    if (!state) {
        return;
    }

    if (delta.location_changed && delta.next_location_id != kLocationUnknown) {
        state->current_location_id = delta.next_location_id;
    }
    if (delta.move_count_changed) {
        state->move_count = delta.next_move_count;
    }
    if (delta.score_changed) {
        state->score = delta.next_score;
    }
    if (delta.alert_level_changed) {
        state->alert_level = delta.next_alert_level;
    }
    if (delta.temperature_changed) {
        state->datacenter_temperature_c = ClampDatacenterTemperatureC(delta.next_datacenter_temperature_c);
    }
    if (delta.cooling_state_changed && delta.next_cooling_state != kResourceUnknown) {
        state->cooling_state = delta.next_cooling_state;
    }
    if (delta.water_state_changed && delta.next_water_state != kResourceUnknown) {
        state->water_state = delta.next_water_state;
    }
    if (delta.power_state_changed && delta.next_power_state != kResourceUnknown) {
        state->power_state = delta.next_power_state;
    }

    for (size_t index = 0; index < delta.inventory_add.size(); ++index) {
        AddUniqueString(&state->inventory_items, delta.inventory_add[index]);
    }
    for (size_t index = 0; index < delta.inventory_remove.size(); ++index) {
        RemoveString(&state->inventory_items, delta.inventory_remove[index]);
    }
    for (size_t index = 0; index < delta.threats_add.size(); ++index) {
        AddUniqueString(&state->unresolved_threats, delta.threats_add[index]);
    }
    for (size_t index = 0; index < delta.threats_remove.size(); ++index) {
        RemoveString(&state->unresolved_threats, delta.threats_remove[index]);
    }

    if (state->move_count < 0) {
        state->move_count = 0;
    }
    if (state->score < 0) {
        state->score = 0;
    }
    state->datacenter_temperature_c = ClampDatacenterTemperatureC(state->datacenter_temperature_c);
}

static void ApplySpatialDelta(SpatialState* state, const SpatialStateDelta& delta)
{
    if (!state) {
        return;
    }

    if (delta.location_changed && delta.next_location_id != kLocationUnknown) {
        SpatialState base_state;
        if (BuildCanonicalSpatialState(delta.next_location_id, &base_state)) {
            *state = base_state;
        } else {
            state->location_id = delta.next_location_id;
        }
    }

    if (delta.time_of_day_changed && delta.next_time_of_day != kTimeUnknown) {
        state->time_of_day = delta.next_time_of_day;
    }
    if (delta.visibility_changed && delta.next_visibility_level != kVisibilityUnknown) {
        state->visibility_level = delta.next_visibility_level;
    }
    if (delta.desert_state_changed && delta.next_desert_state != kDesertUnknown) {
        state->desert_state = delta.next_desert_state;
    }
    if (delta.interior_density_changed && delta.next_interior_density != kInteriorUnknown) {
        state->interior_density = delta.next_interior_density;
    }
    if (delta.alert_level_changed) {
        state->alert_level = delta.next_alert_level;
    }

    if (delta.anchors_present_changed) {
        state->anchors = delta.anchors_present;
    }
    if (delta.visible_objects_changed) {
        state->visible_objects = delta.visible_objects;
    }
    if (delta.blocked_exits_changed) {
        state->blocked_exits = delta.blocked_exits;
    }
    if (delta.spatial_anomalies_changed) {
        state->spatial_anomalies = delta.spatial_anomalies;
    }
    if (delta.scene_constraints_changed) {
        state->scene_constraints = delta.scene_constraints;
    }
}

static std::string ToLowerAsciiCopy(const std::string& text)
{
    std::string lower = text;
    for (size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] >= 'A' && lower[index] <= 'Z') {
            lower[index] = static_cast<char>(lower[index] - 'A' + 'a');
        }
    }
    return lower;
}

static void AppendSearchTerms(std::string* text, const std::vector<std::string>& values)
{
    if (!text) {
        return;
    }

    for (size_t index = 0; index < values.size(); ++index) {
        if (!values[index].empty()) {
            text->push_back(' ');
            text->append(values[index]);
        }
    }
}

static std::string BuildSpatialSemanticText(const SpatialState& spatial_state)
{
    std::string text =
        spatial_state.room_title + " " +
        spatial_state.room_summary + " " +
        spatial_state.location_archetype + " " +
        spatial_state.canonical_fixture;
    AppendSearchTerms(&text, spatial_state.anchors);
    AppendSearchTerms(&text, spatial_state.visible_objects);
    AppendSearchTerms(&text, spatial_state.scene_constraints);
    return ToLowerAsciiCopy(text);
}

static void AppendShortSentenceIfMissing(std::string* text, const char* sentence)
{
    if (!text || !sentence || !sentence[0]) {
        return;
    }

    const std::string lower_text = ToLowerAsciiCopy(*text);
    const std::string lower_sentence = ToLowerAsciiCopy(sentence);
    if (lower_text.find(lower_sentence) != std::string::npos) {
        return;
    }

    if (!text->empty()) {
        const char last = (*text)[text->size() - 1];
        if (last != '.' && last != '!' && last != '?') {
            text->push_back('.');
        }
        text->push_back(' ');
    }
    text->append(sentence);
}

static bool ListContainsSubstring(const std::vector<std::string>& values, const char* pattern);

static bool SpatialFeelsOpenDesert(const SpatialState& state)
{
    return strcmp(DescribeSpatialWorldBand(state), "open desert") == 0;
}

static bool SpatialFeelsOuterParapet(const SpatialState& state)
{
    return strcmp(DescribeSpatialWorldBand(state), "outer parapet") == 0;
}

static bool SpatialFeelsPerimeterSeam(const SpatialState& state)
{
    return strcmp(DescribeSpatialWorldBand(state), "perimeter seam") == 0;
}

static bool ContainsAnySubstring(const std::string& lower_text, const char* const* patterns, size_t pattern_count)
{
    for (size_t index = 0; index < pattern_count; ++index) {
        if (patterns[index] && lower_text.find(patterns[index]) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static void RemoveStringsMatchingAny(
    std::vector<std::string>* values,
    const char* const* patterns,
    size_t pattern_count)
{
    if (!values || !patterns || pattern_count == 0) {
        return;
    }

    for (size_t index = 0; index < values->size();) {
        const std::string lower = ToLowerAsciiCopy((*values)[index]);
        if (ContainsAnySubstring(lower, patterns, pattern_count)) {
            values->erase(values->begin() + index);
            continue;
        }
        ++index;
    }
}

static void EnforceSpatialObjectEnvironmentRules(
    SpatialState* spatial_state,
    bool exterior,
    bool desert,
    bool threshold_like)
{
    if (!spatial_state) {
        return;
    }

    static const char* kExteriorInteriorObjectPatterns[] = {
        "ai server", "mainframe", "accelerator", "gpu", "tensor", "inference",
        "rack", "server", "pod",
        "cooling keypad", "service panel", "cabinet latch", "console", "pedestal", "desk",
        "locker", "cabinet"
    };
    static const char* kExteriorInteriorConstraintPatterns[] = {
        "hero ai server", "rack bank", "cooling flank", "central console", "keep corridor clear", "access door"
    };
    static const char* kOpenExteriorAccessControlPatterns[] = {
        "badge reader", "intercom", "keypad", "service panel", "cabinet latch", "reader"
    };
    static const char* kDesertRemovalPatterns[] = {
        "badge reader", "intercom", "keypad", "service panel", "cabinet latch", "reader",
        "cooling", "vent", "chiller", "hvac", "switch", "gate"
    };
    static const char* kDesertConstraintRemovalPatterns[] = {
        "checkpoint gate", "roof parapet", "perimeter seam"
    };

    if (exterior) {
        RemoveStringsMatchingAny(
            &spatial_state->visible_objects,
            kExteriorInteriorObjectPatterns,
            sizeof(kExteriorInteriorObjectPatterns) / sizeof(kExteriorInteriorObjectPatterns[0]));
        RemoveStringsMatchingAny(
            &spatial_state->scene_constraints,
            kExteriorInteriorConstraintPatterns,
            sizeof(kExteriorInteriorConstraintPatterns) / sizeof(kExteriorInteriorConstraintPatterns[0]));

        if (!threshold_like) {
            RemoveStringsMatchingAny(
                &spatial_state->visible_objects,
                kOpenExteriorAccessControlPatterns,
                sizeof(kOpenExteriorAccessControlPatterns) / sizeof(kOpenExteriorAccessControlPatterns[0]));
        }
    }

    if (desert) {
        RemoveStringsMatchingAny(
            &spatial_state->visible_objects,
            kDesertRemovalPatterns,
            sizeof(kDesertRemovalPatterns) / sizeof(kDesertRemovalPatterns[0]));
        RemoveStringsMatchingAny(
            &spatial_state->scene_constraints,
            kDesertConstraintRemovalPatterns,
            sizeof(kDesertConstraintRemovalPatterns) / sizeof(kDesertConstraintRemovalPatterns[0]));
    }
}

static void EnsureActionableVisibleObjects(SpatialState* spatial_state)
{
    if (!spatial_state) {
        return;
    }

    const std::string combined = BuildSpatialSemanticText(*spatial_state);
    const bool exterior =
        combined.find("exterior") != std::string::npos ||
        combined.find("roof") != std::string::npos ||
        combined.find("gate") != std::string::npos ||
        combined.find("desert") != std::string::npos ||
        combined.find("watch") != std::string::npos ||
        combined.find("perimeter") != std::string::npos ||
        combined.find("parapet") != std::string::npos ||
        combined.find("horizon") != std::string::npos ||
        combined.find("sky") != std::string::npos ||
        SpatialFeelsOuterParapet(*spatial_state) ||
        SpatialFeelsOpenDesert(*spatial_state);
    const bool desert = SpatialFeelsOpenDesert(*spatial_state);
    const bool perimeter = SpatialFeelsPerimeterSeam(*spatial_state) || SpatialFeelsOuterParapet(*spatial_state);
    const bool threshold_like =
        combined.find("gate") != std::string::npos ||
        combined.find("threshold") != std::string::npos ||
        combined.find("checkpoint") != std::string::npos ||
        combined.find("portal") != std::string::npos ||
        combined.find("entry") != std::string::npos;

    if (desert) {
        AddUniqueString(&spatial_state->visible_objects, "survey cache");
        AddUniqueString(&spatial_state->visible_objects, "range marker");
        AddUniqueString(&spatial_state->visible_objects, "buried service hatch");
        AddUniqueString(&spatial_state->visible_objects, "wind-torn sign");
        AddUniqueString(&spatial_state->visible_objects, "rock outcrop");
        AddUniqueString(&spatial_state->visible_objects, "cactus cluster");
    } else if (exterior || perimeter) {
        AddUniqueString(&spatial_state->visible_objects, "warning placard");
        AddUniqueString(&spatial_state->visible_objects, "service crate");
        AddUniqueString(&spatial_state->visible_objects, "maintenance hatch");
    } else {
        AddUniqueString(&spatial_state->visible_objects, "service panel");
        AddUniqueString(&spatial_state->visible_objects, "maintenance crate");
        AddUniqueString(&spatial_state->visible_objects, "cooling keypad");
        AddUniqueString(&spatial_state->visible_objects, "rack access door");
        AddUniqueString(&spatial_state->visible_objects, "cabinet latch");
    }

    if (ListContainsSubstring(spatial_state->visible_objects, "ai server") ||
        ListContainsSubstring(spatial_state->visible_objects, "mainframe") ||
        ListContainsSubstring(spatial_state->visible_objects, "gpu") ||
        ListContainsSubstring(spatial_state->visible_objects, "accelerator") ||
        ListContainsSubstring(spatial_state->visible_objects, "inference")) {
        AddUniqueString(&spatial_state->scene_constraints, "hero ai server");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "rack") ||
        ListContainsSubstring(spatial_state->visible_objects, "server") ||
        ListContainsSubstring(spatial_state->visible_objects, "pod")) {
        AddUniqueString(&spatial_state->scene_constraints, "rack bank");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "cooling") ||
        ListContainsSubstring(spatial_state->visible_objects, "vent") ||
        ListContainsSubstring(spatial_state->visible_objects, "chiller") ||
        ListContainsSubstring(spatial_state->visible_objects, "hvac")) {
        AddUniqueString(&spatial_state->scene_constraints, "cooling flank");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "console") ||
        ListContainsSubstring(spatial_state->visible_objects, "pedestal") ||
        ListContainsSubstring(spatial_state->visible_objects, "desk")) {
        AddUniqueString(&spatial_state->scene_constraints, "central console");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "hatch")) {
        AddUniqueString(&spatial_state->scene_constraints, "rear hatch");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "crate") ||
        ListContainsSubstring(spatial_state->visible_objects, "lockbox") ||
        ListContainsSubstring(spatial_state->visible_objects, "box") ||
        ListContainsSubstring(spatial_state->visible_objects, "case") ||
        ListContainsSubstring(spatial_state->visible_objects, "spool")) {
        AddUniqueString(&spatial_state->scene_constraints, "service crate");
    }
    if (ListContainsSubstring(spatial_state->visible_objects, "gate") ||
        ListContainsSubstring(spatial_state->visible_objects, "door") ||
        ListContainsSubstring(spatial_state->visible_objects, "portal")) {
        AddUniqueString(&spatial_state->scene_constraints, exterior ? "checkpoint gate" : "access door");
    }
    if (desert) {
        AddUniqueString(&spatial_state->scene_constraints, "open horizon");
        AddUniqueString(&spatial_state->scene_constraints, "desert scatter");
        AddUniqueString(&spatial_state->scene_constraints, "rock outcrop");
    } else if (exterior || perimeter) {
        AddUniqueString(&spatial_state->scene_constraints, "open horizon");
        if (perimeter) {
            AddUniqueString(&spatial_state->scene_constraints, "perimeter seam");
        }
    } else {
        AddUniqueString(&spatial_state->scene_constraints, "keep corridor clear");
    }

    EnforceSpatialObjectEnvironmentRules(spatial_state, exterior || perimeter, desert, threshold_like);
}

static bool TryParseTraversalCommand(const char* player_command, CardinalDirection* direction)
{
    if (!direction) {
        return false;
    }

    const std::string normalized = ToLowerAsciiCopy(CollapseWhitespaceCopy(player_command ? player_command : ""));
    if (ParseCardinalDirection(normalized.c_str(), direction)) {
        return true;
    }

    const char* prefixes[] = {"go ", "move ", "walk "};
    for (size_t index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        const std::string prefix = prefixes[index];
        if (normalized.compare(0, prefix.size(), prefix) == 0) {
            return ParseCardinalDirection(normalized.substr(prefix.size()).c_str(), direction);
        }
    }

    *direction = kDirectionUnknown;
    return false;
}

static bool SpatialStateBlocksDirection(const SpatialState& state, CardinalDirection direction)
{
    for (size_t index = 0; index < state.blocked_exits.size(); ++index) {
        CardinalDirection blocked_direction = kDirectionUnknown;
        if (ParseCardinalDirection(state.blocked_exits[index].c_str(), &blocked_direction) && blocked_direction == direction) {
            return true;
        }
    }
    return false;
}

static std::string BuildGeneratedPlaceId(int room_index)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "generated:room_%04d", room_index > 0 ? room_index : 1);
    return buffer;
}

static SpatialState BuildProspectiveSpatialStateForTraversal(
    const SessionState& session_state,
    const std::string& from_place_id,
    CardinalDirection direction)
{
    SpatialState prospective;
    prospective.location_id = kLocationUnknown;
    prospective.time_of_day = session_state.spatial_state.time_of_day;
    prospective.visibility_level = session_state.spatial_state.visibility_level;
    prospective.desert_state = session_state.spatial_state.desert_state;
    prospective.interior_density = session_state.spatial_state.interior_density;
    prospective.alert_level = session_state.spatial_state.alert_level > 0
        ? session_state.spatial_state.alert_level
        : session_state.hard_state.alert_level;
    AssignWorldPoseFromTraversal(session_state, from_place_id, direction, &prospective);
    return prospective;
}

static const GeneratedRoom* FindGeneratedRoomById(const SessionState& session_state, const std::string& room_id)
{
    for (size_t index = 0; index < session_state.generated_rooms.size(); ++index) {
        if (session_state.generated_rooms[index].room_id == room_id) {
            return &session_state.generated_rooms[index];
        }
    }
    return 0;
}

static bool FindRoomLinkTarget(
    const SessionState& session_state,
    const std::string& from_place_id,
    CardinalDirection direction,
    std::string* to_place_id)
{
    if (!to_place_id) {
        return false;
    }

    for (size_t index = 0; index < session_state.room_links.size(); ++index) {
        const RoomLink& link = session_state.room_links[index];
        if (link.from_place_id == from_place_id && link.direction == direction) {
            *to_place_id = link.to_place_id;
            return true;
        }
    }

    return false;
}

static void AddRoomLinkUnique(
    std::vector<RoomLink>* room_links,
    const std::string& from_place_id,
    CardinalDirection direction,
    const std::string& to_place_id)
{
    if (!room_links || from_place_id.empty() || to_place_id.empty() || direction == kDirectionUnknown) {
        return;
    }

    for (size_t index = 0; index < room_links->size(); ++index) {
        if ((*room_links)[index].from_place_id == from_place_id && (*room_links)[index].direction == direction) {
            (*room_links)[index].to_place_id = to_place_id;
            return;
        }
    }

    RoomLink link;
    link.from_place_id = from_place_id;
    link.direction = direction;
    link.to_place_id = to_place_id;
    room_links->push_back(link);
}

static void AddGeneratedRoomUnique(std::vector<GeneratedRoom>* rooms, const GeneratedRoom& room)
{
    if (!rooms || room.room_id.empty()) {
        return;
    }

    for (size_t index = 0; index < rooms->size(); ++index) {
        if ((*rooms)[index].room_id == room.room_id) {
            (*rooms)[index] = room;
            return;
        }
    }

    rooms->push_back(room);
}

static bool SpatialFeelsExterior(const SpatialState& state)
{
    if (SpatialFeelsOuterParapet(state) || SpatialFeelsOpenDesert(state)) {
        return true;
    }

    const std::string combined = BuildSpatialSemanticText(state);
    return combined.find("exterior") != std::string::npos ||
        combined.find("roof") != std::string::npos ||
        combined.find("gate") != std::string::npos ||
        combined.find("yard") != std::string::npos ||
        combined.find("perimeter") != std::string::npos ||
        combined.find("desert") != std::string::npos ||
        combined.find("threshold") != std::string::npos ||
        combined.find("watch") != std::string::npos ||
        combined.find("parapet") != std::string::npos ||
        combined.find("horizon") != std::string::npos ||
        combined.find("sky") != std::string::npos;
}

static bool ListContainsSubstring(const std::vector<std::string>& values, const char* pattern)
{
    if (!pattern) {
        return false;
    }

    const std::string lower_pattern = ToLowerAsciiCopy(pattern);
    for (size_t index = 0; index < values.size(); ++index) {
        if (ToLowerAsciiCopy(values[index]).find(lower_pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

static bool SpatialHasAiServerCue(const SpatialState& spatial_state)
{
    return ListContainsSubstring(spatial_state.visible_objects, "ai server") ||
        ListContainsSubstring(spatial_state.visible_objects, "mainframe") ||
        ListContainsSubstring(spatial_state.visible_objects, "gpu") ||
        ListContainsSubstring(spatial_state.visible_objects, "accelerator") ||
        ListContainsSubstring(spatial_state.visible_objects, "inference") ||
        ListContainsSubstring(spatial_state.scene_constraints, "ai server") ||
        ListContainsSubstring(spatial_state.scene_constraints, "mainframe") ||
        ListContainsSubstring(spatial_state.scene_constraints, "gpu") ||
        ListContainsSubstring(spatial_state.scene_constraints, "accelerator") ||
        ListContainsSubstring(spatial_state.scene_constraints, "inference");
}

static bool SpatialSuggestsAiServer(const SpatialState& spatial_state)
{
    const std::string combined = BuildSpatialSemanticText(spatial_state);
    const bool strong_ai =
        combined.find("inference") != std::string::npos ||
        combined.find("mainframe") != std::string::npos ||
        combined.find("accelerator") != std::string::npos ||
        combined.find("gpu") != std::string::npos ||
        combined.find("tensor") != std::string::npos ||
        combined.find("model") != std::string::npos ||
        combined.find("neural") != std::string::npos ||
        combined.find("compute") != std::string::npos ||
        combined.find("cluster") != std::string::npos ||
        combined.find("training") != std::string::npos;
    const bool compute_space =
        strong_ai ||
        combined.find("server") != std::string::npos ||
        combined.find("vault") != std::string::npos ||
        combined.find("backup") != std::string::npos ||
        combined.find("control") != std::string::npos ||
        combined.find("switch") != std::string::npos;
    const bool cooling_dominant =
        combined.find("cooling") != std::string::npos ||
        combined.find("vent") != std::string::npos ||
        combined.find("chiller") != std::string::npos ||
        combined.find("hvac") != std::string::npos ||
        combined.find("trench") != std::string::npos;
    return compute_space && !cooling_dominant;
}

static void ApplyAiServerBiasToDraft(GeneratedRoomDraft* draft)
{
    if (!draft || SpatialFeelsExterior(draft->spatial_state) || SpatialHasAiServerCue(draft->spatial_state)) {
        return;
    }
    if (!SpatialSuggestsAiServer(draft->spatial_state)) {
        return;
    }

    AddUniqueString(&draft->spatial_state.visible_objects, "inference mainframe");
    AddUniqueString(&draft->spatial_state.visible_objects, "diagnostic console");
    AddUniqueString(&draft->spatial_state.scene_constraints, "hero ai server");
    AppendShortSentenceIfMissing(&draft->summary, "A dark inference mainframe dominates one side.");
    AppendShortSentenceIfMissing(&draft->arrival_narration, "A dark inference mainframe dominates one side.");
}

static void ApplyPerimeterSkyBiasToDraft(GeneratedRoomDraft* draft)
{
    if (!draft) {
        return;
    }

    draft->spatial_state.location_archetype = "roof parapet exterior";
    draft->spatial_state.interior_density = kInteriorSparse;
    AddUniqueString(&draft->spatial_state.anchors, "parapet");
    AddUniqueString(&draft->spatial_state.anchors, "horizon");
    AddUniqueString(&draft->spatial_state.anchors, "roof_edge");
    AddUniqueString(&draft->spatial_state.visible_objects, "roof hatch");
    AddUniqueString(&draft->spatial_state.visible_objects, "warning beacon");
    AddUniqueString(&draft->spatial_state.scene_constraints, "open horizon");
    AddUniqueString(&draft->spatial_state.scene_constraints, "roof parapet");
    RemoveString(&draft->spatial_state.scene_constraints, "keep corridor clear");
    AppendShortSentenceIfMissing(&draft->summary, "The parapet opens to a strip of sky and horizon.");
    AppendShortSentenceIfMissing(&draft->arrival_narration, "Beyond the concrete lip, the sky and horizon are visible.");
}

static void ApplyOpenDesertBiasToDraft(GeneratedRoomDraft* draft)
{
    if (!draft) {
        return;
    }

    draft->spatial_state.location_archetype = "desert perimeter exterior";
    draft->spatial_state.interior_density = kInteriorSparse;
    AddUniqueString(&draft->spatial_state.anchors, "desert");
    AddUniqueString(&draft->spatial_state.anchors, "horizon");
    AddUniqueString(&draft->spatial_state.anchors, "open_sky");
    AddUniqueString(&draft->spatial_state.visible_objects, "survey cache");
    AddUniqueString(&draft->spatial_state.visible_objects, "range marker");
    AddUniqueString(&draft->spatial_state.visible_objects, "buried service hatch");
    AddUniqueString(&draft->spatial_state.visible_objects, "rock outcrop");
    AddUniqueString(&draft->spatial_state.visible_objects, "cactus cluster");
    AddUniqueString(&draft->spatial_state.scene_constraints, "open horizon");
    AddUniqueString(&draft->spatial_state.scene_constraints, "desert scatter");
    AddUniqueString(&draft->spatial_state.scene_constraints, "rock outcrop");
    RemoveString(&draft->spatial_state.scene_constraints, "keep corridor clear");
    AppendShortSentenceIfMissing(&draft->summary, "The datacenter falls away into exposed desert and open sky.");
    AppendShortSentenceIfMissing(&draft->arrival_narration, "Wind crosses open ground beyond the datacenter edge.");
}

static void ApplyGeneratedRoomWorldBiases(GeneratedRoomDraft* draft, CardinalDirection direction)
{
    if (!draft) {
        return;
    }

    ApplyAiServerBiasToDraft(draft);
    if (SpatialFeelsOpenDesert(draft->spatial_state)) {
        ApplyOpenDesertBiasToDraft(draft);
    } else if (SpatialFeelsOuterParapet(draft->spatial_state) || SpatialFeelsPerimeterSeam(draft->spatial_state)) {
        ApplyPerimeterSkyBiasToDraft(draft);
    }
    FinalizeGeneratedRoomDraft(draft, direction);
}

static GeneratedRoomDraft MakeFallbackGeneratedRoomDraft(
    const SessionState& initial_session_state,
    CardinalDirection direction,
    const SpatialState& prospective_spatial_state)
{
    GeneratedRoomDraft draft;
    const SpatialState& origin = initial_session_state.spatial_state;
    const bool desert = SpatialFeelsOpenDesert(prospective_spatial_state);
    const bool exterior =
        desert || SpatialFeelsOuterParapet(prospective_spatial_state) ||
        SpatialFeelsPerimeterSeam(prospective_spatial_state) ||
        SpatialFeelsExterior(origin);

    draft.spatial_state.location_id = kLocationUnknown;
    draft.spatial_state.canonical_fixture.clear();
    draft.spatial_state.world_pose_known = prospective_spatial_state.world_pose_known;
    draft.spatial_state.world_x = prospective_spatial_state.world_x;
    draft.spatial_state.world_z = prospective_spatial_state.world_z;
    draft.spatial_state.time_of_day = origin.time_of_day;
    draft.spatial_state.visibility_level = origin.visibility_level;
    draft.spatial_state.desert_state = origin.desert_state;
    draft.spatial_state.interior_density = exterior ? kInteriorSparse : kInteriorDense;
    draft.spatial_state.alert_level = origin.alert_level > 0 ? origin.alert_level : initial_session_state.hard_state.alert_level;

    switch (direction) {
    case kDirectionNorth:
        draft.title = desert ? "North Desert Reach" : (exterior ? "North Perimeter Walk" : "North Service Bay");
        draft.summary = desert
            ? "Open ground stretching away from the datacenter, with wind, markers and scattered rock."
            : (exterior
            ? "A narrow exterior walk skirting the datacenter shell, open to dust and horizon."
            : "A service bay beyond the main route, lined with maintenance hardware and heavy shadow.");
        break;
    case kDirectionEast:
        draft.title = desert ? "East Desert Margin" : (exterior ? "East Service Yard" : "East Switching Hall");
        draft.summary = desert
            ? "A dry flank where the compound gives way to sand, range markers and sparse cover."
            : (exterior
            ? "An exposed yard of crates, conduit and service fixtures at the edge of the compound."
            : "A lateral switching hall with conduit runs, racks and a hard industrial silence.");
        break;
    case kDirectionSouth:
        draft.title = desert ? "South Dune Cut" : (exterior ? "South Ramp" : "South Cooling Trench");
        draft.summary = desert
            ? "A shallow cut in the sand near buried service hardware and a half-lost warning sign."
            : (exterior
            ? "A sloped service ramp where the compound thins toward the outer ground."
            : "A cooling trench and access lane where airflow and machinery dominate the room.");
        break;
    case kDirectionWest:
        draft.title = desert ? "West Wind Margin" : (exterior ? "West Maintenance Apron" : "West Access Lane");
        draft.summary = desert
            ? "A wind-scoured margin of stone, cactus silhouettes and abandoned service traces."
            : (exterior
            ? "A maintenance apron of parapets, housings and scattered equipment facing the darkening desert."
            : "An access lane between utility blocks, with spare equipment stacked against the walls.");
        break;
    default:
        draft.title = desert ? "Outer Desert" : (exterior ? "Peripheral Exterior" : "Utility Interior");
        draft.summary = desert
            ? "Open desert beyond the datacenter perimeter."
            : (exterior
            ? "A sparse exterior threshold around the datacenter perimeter."
            : "A utility interior neighboring the current room.");
        break;
    }

    draft.arrival_narration = std::string("You move ") + CardinalDirectionToString(direction) + ". " + draft.summary;
    draft.spatial_state.room_title = draft.title;
    draft.spatial_state.room_summary = draft.summary;
    draft.spatial_state.location_archetype =
        desert ? "generated_desert_exterior" : (exterior ? "generated_perimeter_space" : "generated_service_interior");
    draft.spatial_state.anchors.push_back(desert ? "desert" : (exterior ? "perimeter" : "service_lane"));
    draft.spatial_state.anchors.push_back(desert ? "horizon" : (exterior ? "compound_wall" : "utility_wall"));
    draft.spatial_state.anchors.push_back(desert ? "open_sky" : (exterior ? "equipment_pad" : "maintenance_lane"));
    if (desert) {
        draft.spatial_state.visible_objects.push_back("survey cache");
        draft.spatial_state.visible_objects.push_back("range marker");
        draft.spatial_state.visible_objects.push_back("rock outcrop");
        draft.spatial_state.scene_constraints.push_back("open horizon");
        draft.spatial_state.scene_constraints.push_back("desert scatter");
    } else if (exterior) {
        draft.spatial_state.visible_objects.push_back("service crate");
        draft.spatial_state.visible_objects.push_back("warning placard");
        draft.spatial_state.visible_objects.push_back("maintenance hatch");
        draft.spatial_state.scene_constraints.push_back("open horizon");
        draft.spatial_state.scene_constraints.push_back("perimeter seam");
    } else {
        draft.spatial_state.visible_objects.push_back("rack access door");
        draft.spatial_state.visible_objects.push_back("service panel");
        draft.spatial_state.visible_objects.push_back("maintenance crate");
        draft.spatial_state.scene_constraints.push_back("keep corridor clear");
        draft.spatial_state.scene_constraints.push_back("rack bank");
    }
    ApplyGeneratedRoomWorldBiases(&draft, direction);
    return draft;
}

static std::string BuildFallbackGeneratedRoomSceneText(const SpatialState& spatial_state)
{
    const bool desert = SpatialFeelsOpenDesert(spatial_state);
    const bool exterior = SpatialFeelsExterior(spatial_state);
    const bool wants_gate =
        ListContainsSubstring(spatial_state.visible_objects, "gate") ||
        ListContainsSubstring(spatial_state.scene_constraints, "gate") ||
        ListContainsSubstring(spatial_state.scene_constraints, "door");
    const bool wants_ai_server =
        ListContainsSubstring(spatial_state.scene_constraints, "ai server") ||
        ListContainsSubstring(spatial_state.scene_constraints, "mainframe") ||
        ListContainsSubstring(spatial_state.scene_constraints, "gpu") ||
        ListContainsSubstring(spatial_state.scene_constraints, "accelerator") ||
        ListContainsSubstring(spatial_state.scene_constraints, "inference");
    const bool wants_racks =
        ListContainsSubstring(spatial_state.visible_objects, "rack") ||
        ListContainsSubstring(spatial_state.scene_constraints, "rack") ||
        ListContainsSubstring(spatial_state.scene_constraints, "server bank");
    std::string text;
    text += "room \"generated room\"\n";

    if (desert) {
        text += "camera eye(0.0,1.82,-9.6) target(0.0,1.18,9.8) up(0.0,1.0,0.0) fov(48.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(36.0) cone(12.0,28.0) intensity(60.0)\n";
        text += "sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(97)\n";
        text += "plane \"ground\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(120.0,170.0) gray(0.13)\n";
        text += "box \"desert_ridge_left\" pos(-5.2,0.55,7.0) size(3.0,1.10,8.2) gray(0.14)\n";
        text += "box \"desert_ridge_right\" pos(5.4,0.50,8.2) size(3.4,1.00,7.8) gray(0.15)\n";
        text += "box \"desert_back_ridge\" pos(0.0,0.72,12.6) size(12.6,1.44,4.8) gray(0.16)\n";
        text += "prefab_cactus_fork \"cactus_watch\" pos(-4.8,1.2,6.4) size(1.2,2.4,1.2) gray(0.25)\n";
        text += "prefab_cactus_cluster \"cactus_patch\" pos(4.2,1.0,8.1) size(1.4,2.0,1.4) gray(0.24)\n";
        text += "prefab_rock_wide \"rock_shelf\" pos(2.8,0.7,5.8) size(2.0,1.4,1.6) gray(0.23)\n";
        text += "prefab_rock_low \"rock_low\" pos(-2.4,0.5,9.2) size(1.6,1.0,1.3) gray(0.22)\n";
        text += "prefab_crate \"survey_cache\" pos(0.6,0.55,3.8) size(1.5,1.0,1.2) gray(0.20) detail(0.30)\n";
    } else if (exterior) {
        text += "camera eye(0.0,1.82,-8.4) target(0.0,1.20,8.2) up(0.0,1.0,0.0) fov(46.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(34.0) cone(12.0,28.0) intensity(72.0)\n";
        text += "sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(91)\n";
        text += "plane \"ground\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(90.0,150.0) gray(0.13)\n";
        text += "box \"wall_back\" pos(0.0,2.1,10.4) size(18.0,4.2,0.8) gray(0.18)\n";
        text += "box \"parapet_left\" pos(-7.2,0.65,0.0) size(0.8,1.3,20.0) gray(0.25)\n";
        text += "box \"parapet_right\" pos(7.2,0.65,0.0) size(0.8,1.3,20.0) gray(0.25)\n";
        text += "box \"parapet_front\" pos(0.0,0.65,1.9) size(14.0,1.3,0.8) gray(0.26)\n";
        if (wants_gate) {
            text += "prefab_gate \"aux_gate\" pos(0.0,1.4,7.2) size(4.6,2.8,0.5) gray(0.28) detail(0.40)\n";
        }
        text += "prefab_crate \"service_crate\" pos(2.6,0.55,-1.8) size(1.8,1.1,1.5) gray(0.20) detail(0.31)\n";
        text += "box \"warning_beacon\" pos(-4.1,1.15,-2.8) size(0.40,2.30,0.40) gray(0.30) emit(2.4)\n";
        text += "box \"maintenance_hatch\" pos(-0.8,0.12,5.8) size(1.8,0.24,1.8) gray(0.22)\n";
    } else {
        text += "camera eye(0.0,1.78,-7.8) target(0.0,1.20,8.4) up(0.0,1.0,0.0) fov(46.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(30.0) cone(12.0,28.0) intensity(78.0)\n";
        text += "plane \"floor\" pos(0.0,0.0,0.0) normal(0.0,1.0,0.0) size(14.0,24.0) gray(0.12)\n";
        text += "plane \"ceiling\" pos(0.0,3.2,0.0) normal(0.0,-1.0,0.0) size(14.0,24.0) gray(0.19)\n";
        text += "box \"wall_back\" pos(0.0,1.6,11.4) size(14.0,3.2,0.6) gray(0.20)\n";
        text += "box \"wall_left\" pos(-6.8,1.6,0.0) size(0.6,3.2,24.0) gray(0.18)\n";
        text += "box \"wall_right\" pos(6.8,1.6,0.0) size(0.6,3.2,24.0) gray(0.18)\n";
        text += "box \"threshold_frame\" pos(0.0,1.5,8.8) size(3.6,3.0,0.45) gray(0.24)\n";
        if (wants_ai_server) {
            text += "prefab_ai_server \"inference_mainframe\" pos(0.0,1.40,3.6) size(1.70,2.80,1.70) gray(0.16) detail(0.28)\n";
        } else if (wants_racks) {
            text += "prefab_rack \"rack_left\" pos(-3.0,1.25,2.8) size(1.8,2.5,3.6) gray(0.18) detail(0.34)\n";
            text += "prefab_rack \"rack_right\" pos(3.0,1.25,4.4) size(1.8,2.5,3.6) gray(0.18) detail(0.34)\n";
        }
        text += "prefab_cooling_unit \"cooling_block\" pos(-4.4,1.0,-2.5) size(1.1,2.0,1.1) gray(0.31) detail(0.39)\n";
        text += "prefab_crate \"maintenance_crate\" pos(2.4,0.55,-1.6) size(1.7,1.0,1.4) gray(0.20) detail(0.30)\n";
    }

    return text;
}

static void EmitSceneProgramText(const HeadlessTurnConfig& config, const std::string& scene_text)
{
    if (!config.stream_callback || scene_text.empty()) {
        return;
    }

    config.stream_callback(
        kHeadlessTurnStreamSceneProgram,
        scene_text.c_str(),
        scene_text.c_str(),
        config.stream_user_data);
}

static bool BuildGeneratedRoomSceneProgram(
    const HeadlessTurnConfig& config,
    const SpatialState& spatial_state,
    std::string* scene_text,
    Scene* scene,
    std::string* scene_source,
    bool* used_fallback,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene_text || !scene || !scene_source || !used_fallback) {
        SetError(error_buffer, error_buffer_size, "Invalid generated scene target: %s", "(null)");
        return false;
    }

    *used_fallback = false;
    *scene_source = "hybrid";

    std::string hybrid_scene_text;
    char hybrid_error[512];
    hybrid_error[0] = '\0';
    if (BuildSceneTextFromSpatialState(spatial_state, &hybrid_scene_text, hybrid_error, sizeof(hybrid_error))) {
        EmitSceneProgramText(config, hybrid_scene_text);
        if (AuditSceneCandidateText(
                "generated_hybrid.scene",
                hybrid_scene_text.c_str(),
                scene,
                error_buffer,
                error_buffer_size)) {
            *scene_text = hybrid_scene_text;
            return true;
        }
    }

    *used_fallback = true;
    *scene_source = "fallback";
    *scene_text = BuildFallbackGeneratedRoomSceneText(spatial_state);
    EmitSceneProgramText(config, *scene_text);
    if (!AuditSceneCandidateText(
            "generated_fallback.scene",
            scene_text->c_str(),
            scene,
            error_buffer,
            error_buffer_size)) {
        if (hybrid_error[0]) {
            SetError(
                error_buffer,
                error_buffer_size,
                "Generated room scene remained invalid after hybrid compile failure: %s",
                hybrid_error);
        }
        return false;
    }

    return true;
}

static bool RefreshGeneratedRoomCache(
    const SessionState& initial_session_state,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!result || !IsGeneratedPlaceId(result->updated_place_id)) {
        return true;
    }

    const GeneratedRoom* existing_room = FindGeneratedRoomById(initial_session_state, result->updated_place_id);
    if (!existing_room) {
        SetError(error_buffer, error_buffer_size, "Unknown generated place: %s", result->updated_place_id.c_str());
        return false;
    }

    GeneratedRoom refreshed_room = *existing_room;
    refreshed_room.spatial_state = result->updated_spatial_state;
    refreshed_room.spatial_state.location_id = kLocationUnknown;
    refreshed_room.spatial_state.canonical_fixture.clear();
    EnsureActionableVisibleObjects(&refreshed_room.spatial_state);

    Scene refreshed_scene;
    std::string scene_source;
    bool scene_fallback_used = false;
    if (!BuildGeneratedRoomSceneProgram(
            config,
            refreshed_room.spatial_state,
            &refreshed_room.scene_text,
            &refreshed_scene,
            &scene_source,
            &scene_fallback_used,
            error_buffer,
            error_buffer_size)) {
        return false;
    }

    refreshed_room.scene_source = scene_source;
    refreshed_room.scene_fallback_used = scene_fallback_used;
    result->generated_rooms_to_add.push_back(refreshed_room);
    result->generated_room_cache_refreshed = true;
    result->generated_room_scene_fallback_used = scene_fallback_used;
    result->generated_room_scene_source = scene_source;
    result->raw_scene_audit_response_text = refreshed_room.scene_text;
    result->rendered_scene = refreshed_scene;
    result->used_candidate_scene_for_render = false;
    return true;
}

static std::string BuildBlockedTraversalNarration(const SpatialState& spatial_state, CardinalDirection direction)
{
    std::string narration = "The way ";
    narration += CardinalDirectionToString(direction);
    narration += " is blocked.";
    if (!spatial_state.room_summary.empty()) {
        narration += " ";
        narration += spatial_state.room_summary;
    }
    return ConstrainNarrationText(narration, 220, 2);
}

static std::string BuildTraversalNarrationForKnownPlace(const SessionState& session_state, const std::string& place_id, CardinalDirection direction)
{
    std::string narration = "You go ";
    narration += CardinalDirectionToString(direction);
    narration += " and enter ";
    narration += DescribePlaceLabel(session_state, place_id);
    narration += ".";

    const GeneratedRoom* room = FindGeneratedRoomById(session_state, place_id);
    if (room && !room->spatial_state.room_summary.empty()) {
        narration += " ";
        narration += room->spatial_state.room_summary;
    }
    return ConstrainNarrationText(narration, 240, 3);
}

static void FinalizeGeneratedRoomDraft(GeneratedRoomDraft* draft, CardinalDirection direction)
{
    if (!draft) {
        return;
    }

    draft->title = ConstrainNarrationText(draft->title, 72, 1);
    if (draft->title.empty()) {
        draft->title = "Generated Room";
    }

    if (draft->summary.empty()) {
        draft->summary = SpatialFeelsExterior(draft->spatial_state)
            ? "A sparse exterior service pocket at the edge of the datacenter."
            : "A compact service room branching away from the current route.";
    }
    draft->summary = ConstrainNarrationText(draft->summary, 160, 2);

    if (draft->arrival_narration.empty()) {
        draft->arrival_narration = draft->summary.empty()
            ? std::string("You move ") + CardinalDirectionToString(direction) + " into " + draft->title + "."
            : draft->summary;
    }
    draft->arrival_narration = ConstrainNarrationText(draft->arrival_narration, 260, 3);
    if (draft->move_cost < 0) {
        draft->move_cost = 0;
    }
    draft->next_datacenter_temperature_c =
        ClampDatacenterTemperatureC(draft->next_datacenter_temperature_c);

    draft->spatial_state.room_title = draft->title;
    draft->spatial_state.room_summary = draft->summary;
    draft->spatial_state.location_id = kLocationUnknown;
    draft->spatial_state.canonical_fixture.clear();
    RemoveBlockedDirection(&draft->spatial_state.blocked_exits, OppositeCardinalDirection(direction));
    EnsureActionableVisibleObjects(&draft->spatial_state);
}

static bool ParseGeneratedSpatialStateNode(const json& node, SpatialState* spatial_state)
{
    if (!spatial_state || !node.is_object()) {
        return false;
    }

    spatial_state->location_id = kLocationUnknown;
    spatial_state->canonical_fixture.clear();
    spatial_state->location_archetype = ReadStringValue(node, "location_archetype");
    ParseTimeOfDay(ReadStringValue(node, "time_of_day").c_str(), &spatial_state->time_of_day);
    ParseVisibilityLevel(ReadStringValue(node, "visibility_level").c_str(), &spatial_state->visibility_level);
    ParseDesertState(ReadStringValue(node, "desert_state").c_str(), &spatial_state->desert_state);
    ParseInteriorDensity(ReadStringValue(node, "interior_density").c_str(), &spatial_state->interior_density);
    spatial_state->alert_level = ReadIntValue(node, "alert_level", spatial_state->alert_level);
    spatial_state->anchors = ReadStringArray(node.contains("anchors") ? node["anchors"] : json());
    spatial_state->visible_objects = ReadStringArray(node.contains("visible_objects") ? node["visible_objects"] : json());
    spatial_state->blocked_exits = ReadStringArray(node.contains("blocked_exits") ? node["blocked_exits"] : json());
    spatial_state->spatial_anomalies = ReadStringArray(node.contains("spatial_anomalies") ? node["spatial_anomalies"] : json());
    spatial_state->scene_constraints = ReadStringArray(node.contains("scene_constraints") ? node["scene_constraints"] : json());
    return true;
}

static bool ParseGeneratedRoomJson(
    const char* json_text,
    CardinalDirection direction,
    GeneratedRoomDraft* draft,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!json_text || !draft) {
        SetError(error_buffer, error_buffer_size, "Invalid generated room JSON: %s", "(null)");
        return false;
    }

    *draft = GeneratedRoomDraft();
    try {
        json root;
        try {
            root = json::parse(json_text);
        } catch (const std::exception&) {
            std::string extracted_json;
            if (!ExtractFirstJsonObject(json_text, &extracted_json)) {
                throw;
            }
            root = json::parse(extracted_json);
        }
        if (!root.is_object()) {
            SetError(error_buffer, error_buffer_size, "Generated room JSON is not an object: %s", "(root)");
            return false;
        }

        draft->title = ReadStringValue(root, "title");
        draft->summary = ReadStringValue(root, "summary");
        draft->arrival_narration = ReadStringValue(root, "arrival_narration");
        draft->move_cost = ReadIntValue(root, "move_cost", draft->move_cost);
        draft->score_delta = ReadIntValue(root, "score_delta", draft->score_delta);
        if (root.contains("next_datacenter_temperature_c") &&
            root["next_datacenter_temperature_c"].is_number_integer()) {
            draft->temperature_changed = true;
            draft->next_datacenter_temperature_c =
                ClampDatacenterTemperatureC(
                    ReadIntValue(root, "next_datacenter_temperature_c", draft->next_datacenter_temperature_c));
        }
        if (root.contains("spatial_state")) {
            ParseGeneratedSpatialStateNode(root["spatial_state"], &draft->spatial_state);
        }

        draft->spatial_state.room_title = draft->title;
        draft->spatial_state.room_summary = draft->summary;
        draft->spatial_state.location_id = kLocationUnknown;
        draft->spatial_state.canonical_fixture.clear();

        if (draft->title.empty()) {
            SetError(error_buffer, error_buffer_size, "Generated room JSON missing title: %s", "(title)");
            return false;
        }
        if (draft->arrival_narration.empty()) {
            draft->arrival_narration = draft->summary.empty()
                ? std::string("You enter ") + draft->title + "."
                : draft->summary;
        }
        FinalizeGeneratedRoomDraft(draft, direction);
        return true;
    } catch (const std::exception& exception) {
        SetError(error_buffer, error_buffer_size, "Failed to parse generated room JSON: %s", exception.what());
        return false;
    }
}

static bool LoadSceneForPlace(
    const SessionState& session_state,
    const std::string& place_id,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene) {
        SetError(error_buffer, error_buffer_size, "Invalid scene target: %s", "(null)");
        return false;
    }

    LocationId location_id = kLocationUnknown;
    if (ParseCanonicalPlaceId(place_id, &location_id)) {
        SpatialState spatial_state;
        if (!BuildCanonicalSpatialState(location_id, &spatial_state)) {
            SetError(error_buffer, error_buffer_size, "Cannot build canonical place: %s", place_id.c_str());
            return false;
        }
        return CompileSpatialStateToScene(spatial_state, scene, error_buffer, error_buffer_size);
    }

    const GeneratedRoom* room = FindGeneratedRoomById(session_state, place_id);
    if (!room) {
        SetError(error_buffer, error_buffer_size, "Unknown generated place: %s", place_id.c_str());
        return false;
    }

    return AuditSceneCandidateText(place_id.c_str(), room->scene_text.c_str(), scene, error_buffer, error_buffer_size);
}

static bool ApplyPlaceToState(
    const SessionState& session_state,
    const std::string& place_id,
    HardState* hard_state,
    SpatialState* spatial_state,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!hard_state || !spatial_state) {
        SetError(error_buffer, error_buffer_size, "Invalid place state target: %s", "(null)");
        return false;
    }

    LocationId location_id = kLocationUnknown;
    if (ParseCanonicalPlaceId(place_id, &location_id)) {
        if (!BuildCanonicalSpatialState(location_id, spatial_state)) {
            SetError(error_buffer, error_buffer_size, "Cannot build canonical place: %s", place_id.c_str());
            return false;
        }
        float world_x = 0.0f;
        float world_z = 0.0f;
        if (GetCanonicalWorldPose(location_id, &world_x, &world_z)) {
            SetSpatialWorldPose(spatial_state, world_x, world_z);
        }
        EnsureActionableVisibleObjects(spatial_state);
        hard_state->current_location_id = location_id;
        if (spatial_state->alert_level > 0) {
            hard_state->alert_level = spatial_state->alert_level;
        }
        return true;
    }

    const GeneratedRoom* room = FindGeneratedRoomById(session_state, place_id);
    if (!room) {
        SetError(error_buffer, error_buffer_size, "Unknown generated place: %s", place_id.c_str());
        return false;
    }

    *spatial_state = room->spatial_state;
    spatial_state->location_id = kLocationUnknown;
    spatial_state->canonical_fixture.clear();
    EnsureActionableVisibleObjects(spatial_state);
    hard_state->current_location_id = kLocationUnknown;
    if (spatial_state->alert_level > 0) {
        hard_state->alert_level = spatial_state->alert_level;
    }
    return true;
}

static std::string DetermineUpdatedPlaceIdAfterStandardTurn(
    const SessionState& initial_session_state,
    const SpatialState& updated_spatial_state)
{
    if (updated_spatial_state.location_id != kLocationUnknown) {
        return BuildCanonicalPlaceId(updated_spatial_state.location_id);
    }
    if (!initial_session_state.current_place_id.empty()) {
        return initial_session_state.current_place_id;
    }
    if (initial_session_state.hard_state.current_location_id != kLocationUnknown) {
        return BuildCanonicalPlaceId(initial_session_state.hard_state.current_location_id);
    }
    return std::string();
}

}  // namespace

bool ParseTurnResultJson(
    const char* json_text,
    TurnResult* turn_result,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!json_text || !turn_result) {
        SetError(error_buffer, error_buffer_size, "Invalid turn JSON: %s", "(null)");
        return false;
    }

    *turn_result = TurnResult();

    try {
        json root;
        try {
            root = json::parse(json_text);
        } catch (const std::exception&) {
            std::string extracted_json;
            if (!ExtractFirstJsonObject(json_text, &extracted_json)) {
                throw;
            }
            root = json::parse(extracted_json);
        }
        if (!root.is_object()) {
            SetError(error_buffer, error_buffer_size, "Turn JSON is not an object: %s", "(root)");
            return false;
        }

        turn_result->intent = ReadStringValue(root, "intent");
        turn_result->narration = ConstrainNarrationText(ReadStringValue(root, "narration"), 280, 3);
        turn_result->clarification = ConstrainNarrationText(ReadStringValue(root, "clarification"), 220, 2);

        if (root.contains("hard_state_delta")) {
            ParseHardStateDeltaNode(root["hard_state_delta"], &turn_result->hard_state_delta);
        }
        if (root.contains("spatial_delta")) {
            ParseSpatialStateDeltaNode(root["spatial_delta"], &turn_result->spatial_delta);
        }
        if (root.contains("continuity_notes")) {
            turn_result->continuity_notes = ReadStringArray(root["continuity_notes"]);
        }
        if (root.contains("candidate_scene_text") && root["candidate_scene_text"].is_string()) {
            turn_result->candidate_scene_text = root["candidate_scene_text"].get<std::string>();
            turn_result->candidate_scene_included = true;
        }

        return true;
    } catch (const std::exception& exception) {
        SetError(error_buffer, error_buffer_size, "Failed to parse turn JSON: %s", exception.what());
        return false;
    }
}

bool InitializeSessionState(
    LocationId initial_location_id,
    SessionState* session_state,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!session_state) {
        SetError(error_buffer, error_buffer_size, "Invalid session state: %s", "(null)");
        return false;
    }

    *session_state = SessionState();
    session_state->hard_state = MakeInitialHardState();
    session_state->soft_state = MakeInitialSoftState();
    session_state->hard_state.current_location_id = initial_location_id;
    session_state->origin_place_id = BuildCanonicalPlaceId(initial_location_id);
    session_state->current_place_id = BuildCanonicalPlaceId(initial_location_id);
    if (!BuildCanonicalSpatialState(initial_location_id, &session_state->spatial_state)) {
        SetError(error_buffer, error_buffer_size, "Cannot build canonical spatial state: %s", LocationIdToString(initial_location_id));
        return false;
    }
    EnsureActionableVisibleObjects(&session_state->spatial_state);
    session_state->hard_state.alert_level = session_state->spatial_state.alert_level;
    NormalizeSessionState(session_state);
    return true;
}

void ApplyTurnResult(
    const TurnResult& turn_result,
    HardState* hard_state,
    SoftState* soft_state,
    SpatialState* spatial_state)
{
    ApplyHardDelta(hard_state, turn_result.hard_state_delta);
    ApplySpatialDelta(spatial_state, turn_result.spatial_delta);

    if (hard_state && spatial_state) {
        if (hard_state->current_location_id == kLocationUnknown && spatial_state->location_id != kLocationUnknown) {
            hard_state->current_location_id = spatial_state->location_id;
        }
        if (spatial_state->location_id == kLocationUnknown && hard_state->current_location_id != kLocationUnknown) {
            spatial_state->location_id = hard_state->current_location_id;
        }
        if (hard_state->current_location_id != spatial_state->location_id && spatial_state->location_id != kLocationUnknown) {
            hard_state->current_location_id = spatial_state->location_id;
        }
        ++hard_state->turn_number;
    }

    if (spatial_state) {
        EnsureActionableVisibleObjects(spatial_state);
    }

    if (soft_state) {
        if (!turn_result.narration.empty()) {
            soft_state->rolling_summary = turn_result.narration;
        }
        if (!turn_result.continuity_notes.empty()) {
            soft_state->tolerated_incoherences = turn_result.continuity_notes;
        }
    }
}

void UpdateSessionStateFromTurn(
    const char* player_command,
    const HeadlessTurnResult& turn_result,
    SessionState* session_state)
{
    if (!session_state) {
        return;
    }

    session_state->hard_state = turn_result.updated_hard_state;
    session_state->soft_state = turn_result.updated_soft_state;
    session_state->spatial_state = turn_result.updated_spatial_state;
    session_state->current_place_id = turn_result.updated_place_id;

    for (size_t index = 0; index < turn_result.generated_rooms_to_add.size(); ++index) {
        bool existed = false;
        for (size_t room_index = 0; room_index < session_state->generated_rooms.size(); ++room_index) {
            if (session_state->generated_rooms[room_index].room_id == turn_result.generated_rooms_to_add[index].room_id) {
                existed = true;
                break;
            }
        }
        AddGeneratedRoomUnique(&session_state->generated_rooms, turn_result.generated_rooms_to_add[index]);
        if (!existed && IsGeneratedPlaceId(turn_result.generated_rooms_to_add[index].room_id)) {
            ++session_state->next_generated_room_index;
        }
    }
    for (size_t index = 0; index < turn_result.room_links_to_add.size(); ++index) {
        AddRoomLinkUnique(
            &session_state->room_links,
            turn_result.room_links_to_add[index].from_place_id,
            turn_result.room_links_to_add[index].direction,
            turn_result.room_links_to_add[index].to_place_id);
    }

    SessionTurnRecord record;
    record.turn_number = turn_result.initial_hard_state.turn_number;
    record.location_id = turn_result.updated_spatial_state.location_id;
    record.location_label = session_state->current_place_id.empty()
        ? (record.location_id == kLocationUnknown ? "unknown" : LocationIdToString(record.location_id))
        : DescribePlaceLabel(*session_state, session_state->current_place_id);
    record.player_command = player_command ? player_command : "";
    record.intent = turn_result.turn_result.intent;
    record.narration = turn_result.turn_result.narration;
    record.clarification = turn_result.turn_result.clarification;
    session_state->history.push_back(record);
    NormalizeSessionState(session_state);
}

bool RunHeadlessTurnFromState(
    const SessionState& initial_session_state,
    const char* player_command,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!result) {
        SetError(error_buffer, error_buffer_size, "Invalid headless turn result: %s", "(null)");
        return false;
    }

    *result = HeadlessTurnResult();
    result->initial_place_id = initial_session_state.current_place_id.empty()
        ? BuildCanonicalPlaceId(initial_session_state.hard_state.current_location_id)
        : initial_session_state.current_place_id;
    result->updated_place_id = result->initial_place_id;
    result->initial_hard_state = initial_session_state.hard_state;
    result->initial_soft_state = initial_session_state.soft_state;
    result->initial_spatial_state = initial_session_state.spatial_state;
    result->updated_hard_state = result->initial_hard_state;
    result->updated_soft_state = result->initial_soft_state;
    result->updated_spatial_state = result->initial_spatial_state;

    CardinalDirection traversal_direction = kDirectionUnknown;
    if (TryParseTraversalCommand(player_command, &traversal_direction)) {
        if (SpatialStateBlocksDirection(result->initial_spatial_state, traversal_direction)) {
            result->turn_result.intent = std::string("move_") + CardinalDirectionToString(traversal_direction) + "_blocked";
            result->turn_result.narration = BuildBlockedTraversalNarration(result->initial_spatial_state, traversal_direction);
            result->turn_result.clarification = "No new room was generated because the current spatial brief marks that exit as blocked.";
            result->updated_soft_state.rolling_summary = result->turn_result.narration;
            ++result->updated_hard_state.turn_number;
            if (!LoadSceneForPlace(initial_session_state, result->updated_place_id, &result->rendered_scene, error_buffer, error_buffer_size)) {
                return false;
            }
            return true;
        }

        std::string linked_place_id;
        if (FindRoomLinkTarget(initial_session_state, result->initial_place_id, traversal_direction, &linked_place_id)) {
            result->turn_result.intent = std::string("move_") + CardinalDirectionToString(traversal_direction) + "_known_room";
            result->turn_result.narration = BuildTraversalNarrationForKnownPlace(initial_session_state, linked_place_id, traversal_direction);
            result->updated_place_id = linked_place_id;
            if (!ApplyPlaceToState(
                    initial_session_state,
                    linked_place_id,
                    &result->updated_hard_state,
                    &result->updated_spatial_state,
                    error_buffer,
                    error_buffer_size)) {
                return false;
            }
            result->updated_soft_state.rolling_summary = result->turn_result.narration;
            ++result->updated_hard_state.move_count;
            ++result->updated_hard_state.turn_number;
            if (!LoadSceneForPlace(initial_session_state, linked_place_id, &result->rendered_scene, error_buffer, error_buffer_size)) {
                return false;
            }
            return true;
        }

        const SpatialState prospective_spatial_state = BuildProspectiveSpatialStateForTraversal(
            initial_session_state,
            result->initial_place_id,
            traversal_direction);

        const std::string room_prompt = BuildGeneratedRoomPrompt(
            result->initial_hard_state,
            result->initial_soft_state,
            result->initial_spatial_state,
            prospective_spatial_state,
            &initial_session_state.history,
            traversal_direction);

        std::vector<LlmPromptMessage> room_messages;
        room_messages.push_back(LlmPromptMessage());
        room_messages.back().role = "system";
        room_messages.back().content =
            "You invent one new neighboring room for a local interactive-fiction prototype. "
            "Return valid JSON only. Do not use markdown fences.";
        room_messages.push_back(LlmPromptMessage());
        room_messages.back().role = "user";
        room_messages.back().content = room_prompt;

        GeneratedRoomDraft draft;
        bool used_room_metadata_fallback = false;
        bool used_room_scene_fallback = false;

        LlmGenerationResult room_generation_result;
        StreamForwarder room_stream_forwarder;
        room_stream_forwarder.callback = config.stream_callback;
        room_stream_forwarder.phase = kHeadlessTurnStreamPrimaryResponse;
        room_stream_forwarder.user_data = config.stream_user_data;
        if (GenerateChatCompletion(
                config.generation_config,
                room_messages,
                config.stream_callback ? ForwardStreamChunk : 0,
                config.stream_callback ? &room_stream_forwarder : 0,
                &room_generation_result)) {
            result->prompt_text = room_generation_result.prompt_text;
            result->raw_response_text = room_generation_result.response_text;
            result->prompt_tokens += room_generation_result.prompt_tokens;
            result->generated_tokens += room_generation_result.generated_tokens;
            result->inference_time_ms += room_generation_result.inference_time_ms;

            char metadata_error[512];
            metadata_error[0] = '\0';
            if (!ParseGeneratedRoomJson(
                    room_generation_result.response_text.c_str(),
                    traversal_direction,
                    &draft,
                    metadata_error,
                    sizeof(metadata_error))) {
                draft = MakeFallbackGeneratedRoomDraft(
                    initial_session_state,
                    traversal_direction,
                    prospective_spatial_state);
                used_room_metadata_fallback = true;
                result->used_turn_fallback = true;
                result->turn_result.clarification = std::string("Generated room metadata fallback was used: ") + metadata_error;
            }
        } else {
            draft = MakeFallbackGeneratedRoomDraft(
                initial_session_state,
                traversal_direction,
                prospective_spatial_state);
            used_room_metadata_fallback = true;
            result->used_turn_fallback = true;
            result->turn_result.clarification =
                std::string("Generated room metadata fallback was used after LLM failure: ") +
                (room_generation_result.error_message.empty() ? "unknown error" : room_generation_result.error_message);
        }

        if (draft.spatial_state.alert_level <= 0) {
            draft.spatial_state.alert_level = result->initial_spatial_state.alert_level > 0
                ? result->initial_spatial_state.alert_level
                : result->initial_hard_state.alert_level;
        }
        if (draft.spatial_state.time_of_day == kTimeUnknown) {
            draft.spatial_state.time_of_day = result->initial_spatial_state.time_of_day;
        }
        if (draft.spatial_state.visibility_level == kVisibilityUnknown) {
            draft.spatial_state.visibility_level = result->initial_spatial_state.visibility_level;
        }
        if (draft.spatial_state.desert_state == kDesertUnknown) {
            draft.spatial_state.desert_state = result->initial_spatial_state.desert_state;
        }
        if (draft.spatial_state.interior_density == kInteriorUnknown) {
            draft.spatial_state.interior_density = result->initial_spatial_state.interior_density;
        }
        if (!draft.spatial_state.world_pose_known) {
            draft.spatial_state.world_pose_known = prospective_spatial_state.world_pose_known;
            draft.spatial_state.world_x = prospective_spatial_state.world_x;
            draft.spatial_state.world_z = prospective_spatial_state.world_z;
        }
        if (draft.spatial_state.room_title.empty()) {
            draft.spatial_state.room_title = draft.title.empty() ? "Generated Room" : draft.title;
        }
        if (draft.spatial_state.room_summary.empty()) {
            draft.spatial_state.room_summary = draft.summary;
        }
        if (draft.arrival_narration.empty()) {
            draft.arrival_narration = draft.summary.empty()
                ? std::string("You move ") + CardinalDirectionToString(traversal_direction) + " into " + draft.spatial_state.room_title + "."
                : draft.summary;
        }
        ApplyGeneratedRoomWorldBiases(&draft, traversal_direction);

        std::string generated_scene_text;
        Scene generated_scene;
        std::string generated_scene_source;
        if (!BuildGeneratedRoomSceneProgram(
                config,
                draft.spatial_state,
                &generated_scene_text,
                &generated_scene,
                &generated_scene_source,
                &used_room_scene_fallback,
                error_buffer,
                error_buffer_size)) {
            return false;
        }
        result->raw_scene_audit_response_text = generated_scene_text;

        GeneratedRoom generated_room;
        generated_room.room_id = BuildGeneratedPlaceId(initial_session_state.next_generated_room_index);
        generated_room.spatial_state = draft.spatial_state;
        generated_room.scene_text = generated_scene_text;
        generated_room.scene_source = generated_scene_source;
        generated_room.metadata_fallback_used = used_room_metadata_fallback;
        generated_room.scene_fallback_used = used_room_scene_fallback;

        result->generated_rooms_to_add.push_back(generated_room);
        result->generated_room_created = true;
        result->generated_room_metadata_fallback_used = used_room_metadata_fallback;
        result->generated_room_scene_fallback_used = used_room_scene_fallback;
        result->generated_room_scene_source = generated_scene_source;
        AddRoomLinkUnique(&result->room_links_to_add, result->initial_place_id, traversal_direction, generated_room.room_id);
        AddRoomLinkUnique(
            &result->room_links_to_add,
            generated_room.room_id,
            OppositeCardinalDirection(traversal_direction),
            result->initial_place_id);

        result->updated_place_id = generated_room.room_id;
        result->updated_hard_state.current_location_id = kLocationUnknown;
        result->updated_soft_state.rolling_summary = draft.arrival_narration;
        result->updated_spatial_state = generated_room.spatial_state;
        result->updated_spatial_state.location_id = kLocationUnknown;
        result->updated_spatial_state.canonical_fixture.clear();
        EnsureActionableVisibleObjects(&result->updated_spatial_state);
        result->updated_hard_state.move_count += draft.move_cost;
        result->updated_hard_state.score += draft.score_delta;
        if (draft.temperature_changed) {
            result->updated_hard_state.datacenter_temperature_c = draft.next_datacenter_temperature_c;
        }
        if (result->updated_hard_state.move_count < 0) {
            result->updated_hard_state.move_count = 0;
        }
        if (result->updated_hard_state.score < 0) {
            result->updated_hard_state.score = 0;
        }
        if (result->updated_spatial_state.alert_level > 0) {
            result->updated_hard_state.alert_level = result->updated_spatial_state.alert_level;
        }
        ++result->updated_hard_state.turn_number;

        result->turn_result.intent = std::string("move_") + CardinalDirectionToString(traversal_direction) + "_generated_room";
        result->turn_result.narration = draft.arrival_narration;
        if (used_room_metadata_fallback && result->turn_result.clarification.empty()) {
            result->turn_result.clarification = "Generated room metadata fallback was used.";
        }
        if (used_room_scene_fallback) {
            if (!result->turn_result.clarification.empty()) {
                result->turn_result.clarification.append(" ");
            }
            result->turn_result.clarification.append("Generated room scene fallback was used.");
        }
        result->rendered_scene = generated_scene;
        return true;
    }

    const std::string user_prompt = BuildTurnPrompt(
        result->initial_hard_state,
        result->initial_soft_state,
        result->initial_spatial_state,
        &initial_session_state.history,
        player_command,
        false);

    std::vector<LlmPromptMessage> messages;
    messages.push_back(LlmPromptMessage());
    messages.back().role = "system";
    messages.back().content =
        "You are a deterministic interactive-fiction turn engine. Return valid JSON only. "
        "Do not use markdown fences. Do not write any text outside the JSON object.";
    messages.push_back(LlmPromptMessage());
    messages.back().role = "user";
    messages.back().content = user_prompt;

    LlmGenerationResult generation_result;
    StreamForwarder turn_stream_forwarder;
    turn_stream_forwarder.callback = config.stream_callback;
    turn_stream_forwarder.phase = kHeadlessTurnStreamPrimaryResponse;
    turn_stream_forwarder.user_data = config.stream_user_data;
    if (!GenerateChatCompletion(
            config.generation_config,
            messages,
            config.stream_callback ? ForwardStreamChunk : 0,
            config.stream_callback ? &turn_stream_forwarder : 0,
            &generation_result)) {
        SetError(
            error_buffer,
            error_buffer_size,
            "LLM generation failed: %s",
            generation_result.error_message.empty() ? "unknown error" : generation_result.error_message.c_str());
        return false;
    }

    result->prompt_text = generation_result.prompt_text;
    result->raw_response_text = generation_result.response_text;
    result->prompt_tokens = generation_result.prompt_tokens;
    result->generated_tokens = generation_result.generated_tokens;
    result->inference_time_ms = generation_result.inference_time_ms;

    if (!ParseTurnResultJson(generation_result.response_text.c_str(), &result->turn_result, error_buffer, error_buffer_size)) {
        char parse_error[512];
        memset(parse_error, 0, sizeof(parse_error));
        if (!RepairTurnResultJson(
                config,
                generation_result.response_text,
                &result->turn_result,
                &result->repair_response_text,
                parse_error,
                sizeof(parse_error))) {
            result->turn_result = MakeFallbackTurnResult(generation_result.response_text);
            result->used_turn_fallback = true;
        } else {
            result->used_turn_repair = true;
        }
    }

    ApplyTurnResult(result->turn_result, &result->updated_hard_state, &result->updated_soft_state, &result->updated_spatial_state);
    result->updated_place_id = DetermineUpdatedPlaceIdAfterStandardTurn(initial_session_state, result->updated_spatial_state);

    if (config.request_candidate_scene && !IsGeneratedPlaceId(result->updated_place_id)) {
        std::vector<LlmPromptMessage> scene_messages;
        scene_messages.push_back(LlmPromptMessage());
        scene_messages.back().role = "system";
        scene_messages.back().content =
            "You are a deterministic scene compiler. Return only a valid .scene program. "
            "Do not use markdown fences. Do not write any explanation.";
        scene_messages.push_back(LlmPromptMessage());
        scene_messages.back().role = "user";
        scene_messages.back().content = BuildSceneAuditPrompt(result->updated_spatial_state);

        LlmGenerationConfig scene_config = config.generation_config;
        scene_config.use_json_grammar = false;

        LlmGenerationResult scene_generation_result;
        StreamForwarder scene_stream_forwarder;
        scene_stream_forwarder.callback = config.stream_callback;
        scene_stream_forwarder.phase = kHeadlessTurnStreamSceneProgram;
        scene_stream_forwarder.user_data = config.stream_user_data;
        if (GenerateChatCompletion(
                scene_config,
                scene_messages,
                config.stream_callback ? ForwardStreamChunk : 0,
                config.stream_callback ? &scene_stream_forwarder : 0,
                &scene_generation_result)) {
            result->raw_scene_audit_response_text = scene_generation_result.response_text;
            result->turn_result.candidate_scene_text = NormalizeCodeBlockText(scene_generation_result.response_text);
            result->turn_result.candidate_scene_included = !result->turn_result.candidate_scene_text.empty();
        }
    }

    if (result->turn_result.candidate_scene_included && !result->turn_result.candidate_scene_text.empty()) {
        char candidate_error[512];
        candidate_error[0] = '\0';
        result->candidate_scene_valid = AuditSceneCandidateText(
            "candidate.scene",
            result->turn_result.candidate_scene_text.c_str(),
            &result->candidate_scene,
            candidate_error,
            sizeof(candidate_error));
        if (!result->candidate_scene_valid) {
            result->candidate_scene_error = candidate_error;
        }
    }

    if (config.prefer_candidate_scene && result->candidate_scene_valid) {
        result->rendered_scene = result->candidate_scene;
        result->used_candidate_scene_for_render = true;
        return true;
    }

    if (IsGeneratedPlaceId(result->updated_place_id)) {
        if (!RefreshGeneratedRoomCache(initial_session_state, config, result, error_buffer, error_buffer_size)) {
            return false;
        }
        return true;
    }

    if (!LoadSceneForPlace(initial_session_state, result->updated_place_id, &result->rendered_scene, error_buffer, error_buffer_size)) {
        return false;
    }

    return true;
}

bool RunHeadlessTurn(
    LocationId initial_location_id,
    const char* player_command,
    const HeadlessTurnConfig& config,
    HeadlessTurnResult* result,
    char* error_buffer,
    size_t error_buffer_size)
{
    SessionState session_state;
    if (!InitializeSessionState(initial_location_id, &session_state, error_buffer, error_buffer_size)) {
        return false;
    }
    return RunHeadlessTurnFromState(session_state, player_command, config, result, error_buffer, error_buffer_size);
}

}  // namespace liminal
