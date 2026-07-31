#include "turn_runner.h"

#include <stdio.h>

#include "scene_compiler.h"
#include "turn_contract.h"
#include "nlohmann/json.hpp"

namespace liminal {

namespace {

using json = nlohmann::json;

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
    delta->alert_level_changed = ReadBoolValue(node, "alert_level_changed", false);
    delta->next_alert_level = ReadIntValue(node, "next_alert_level", 0);
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
    if (delta.alert_level_changed) {
        state->alert_level = delta.next_alert_level;
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

    state->anchors = delta.anchors_present;
    state->visible_objects = delta.visible_objects;
    state->blocked_exits = delta.blocked_exits;
    state->spatial_anomalies = delta.spatial_anomalies;
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
        turn_result->narration = ReadStringValue(root, "narration");
        turn_result->clarification = ReadStringValue(root, "clarification");

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

    if (soft_state) {
        if (!turn_result.narration.empty()) {
            soft_state->rolling_summary = turn_result.narration;
        }
        if (!turn_result.continuity_notes.empty()) {
            soft_state->tolerated_incoherences = turn_result.continuity_notes;
        }
    }
}

bool RunHeadlessTurn(
    LocationId initial_location_id,
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
    result->initial_hard_state = MakeInitialHardState();
    result->initial_hard_state.current_location_id = initial_location_id;
    result->initial_soft_state = MakeInitialSoftState();
    if (!BuildCanonicalSpatialState(initial_location_id, &result->initial_spatial_state)) {
        SetError(error_buffer, error_buffer_size, "Cannot build canonical spatial state: %s", LocationIdToString(initial_location_id));
        return false;
    }
    result->initial_hard_state.alert_level = result->initial_spatial_state.alert_level;

    const std::string user_prompt = BuildTurnPrompt(
        result->initial_hard_state,
        result->initial_soft_state,
        result->initial_spatial_state,
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
    if (!GenerateChatCompletion(config.generation_config, messages, &generation_result)) {
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
        return false;
    }

    result->updated_hard_state = result->initial_hard_state;
    result->updated_soft_state = result->initial_soft_state;
    result->updated_spatial_state = result->initial_spatial_state;
    ApplyTurnResult(result->turn_result, &result->updated_hard_state, &result->updated_soft_state, &result->updated_spatial_state);

    if (config.request_candidate_scene) {
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
        if (GenerateChatCompletion(scene_config, scene_messages, &scene_generation_result)) {
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

    if (!CompileSpatialStateToScene(result->updated_spatial_state, &result->rendered_scene, error_buffer, error_buffer_size)) {
        return false;
    }

    return true;
}

}  // namespace liminal
