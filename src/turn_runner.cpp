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
    int next_spatial_entropy;
    int next_external_temperature_c;
    float next_body_temperature_c;
    SpatialState spatial_state;

    GeneratedRoomDraft()
        : move_cost(1)
        , score_delta(0)
        , next_spatial_entropy(kDefaultSpatialEntropy)
        , next_external_temperature_c(kDefaultExternalTemperatureC)
        , next_body_temperature_c(kDefaultBodyTemperatureC)
    {
    }
};

static const char* PlayerText(
    GameLanguage language,
    const char* english,
    const char* french,
    const char* norwegian,
    const char* danish,
    const char* german,
    const char* italian)
{
    switch (language) {
    case kGameLanguageFrench: return french;
    case kGameLanguageNorwegian: return norwegian;
    case kGameLanguageDanish: return danish;
    case kGameLanguageGerman: return german;
    case kGameLanguageItalian: return italian;
    case kGameLanguageEnglish:
    default: return english;
    }
}

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

static TurnResult MakeFallbackTurnResult(const std::string& raw_response_text, GameLanguage language)
{
    TurnResult turn_result;
    turn_result.intent = "fallback_noop";
    ExtractQuotedJsonField(raw_response_text, "intent", &turn_result.intent);
    turn_result.narration = language == kGameLanguageEnglish
        ? BuildFallbackNarration(raw_response_text)
        : PlayerText(
            language,
            "The system response remained unreadable. Your position and the world state do not change.",
            "La réponse du système est restée illisible. Votre position et l'état du monde ne changent pas.",
            "Systemsvaret forble uleselig. Posisjonen din og verdenstilstanden endres ikke.",
            "Systemets svar forblev ulæseligt. Din position og verdens tilstand ændres ikke.",
            "Die Systemantwort blieb unlesbar. Deine Position und der Weltzustand bleiben unverändert.",
            "La risposta del sistema è rimasta illeggibile. La tua posizione e lo stato del mondo non cambiano.");
    turn_result.clarification = PlayerText(
        language,
        "The world state was kept unchanged after a malformed model response.",
        "L'état du monde a été conservé après une réponse mal formée du modèle.",
        "Verdenstilstanden ble bevart etter et ugyldig modellsvar.",
        "Verdens tilstand blev bevaret efter et ugyldigt modelsvar.",
        "Der Weltzustand wurde nach einer fehlerhaften Modellantwort beibehalten.",
        "Lo stato del mondo è stato conservato dopo una risposta non valida del modello.");
    turn_result.continuity_notes.push_back("Malformed model response was ignored; hard state and spatial state were kept stable.");
    return turn_result;
}

static void CaptureSceneDebugArtifacts(
    const SpatialState& spatial_state,
    const std::string* preferred_scene_text,
    std::string* scene_text,
    std::string* scene_debug_text)
{
    if (scene_text) {
        scene_text->clear();
    }
    if (scene_debug_text) {
        scene_debug_text->clear();
    }

    char debug_error[512];
    debug_error[0] = '\0';

    if (scene_text) {
        if (preferred_scene_text) {
            *scene_text = *preferred_scene_text;
        } else {
            std::string generated_text;
            if (BuildSceneTextFromSpatialState(
                    spatial_state,
                    &generated_text,
                    debug_error,
                    sizeof(debug_error))) {
                *scene_text = generated_text;
            }
        }
    }

    if (scene_debug_text) {
        std::string report_text;
        if (BuildSceneDebugReportFromSpatialState(
                spatial_state,
                &report_text,
                debug_error,
                sizeof(debug_error))) {
            *scene_debug_text = report_text;
        } else if (debug_error[0]) {
            *scene_debug_text = std::string("Scene debug report unavailable: ") + debug_error;
        }
    }
}

static void PrintDebugBlock(FILE* stream, const char* title, const std::string& text)
{
    FILE* out = stream ? stream : stdout;
    if (!title || !title[0]) {
        return;
    }

    fprintf(out, "\n=== %s ===\n", title);
    if (text.empty()) {
        fprintf(out, "(empty)\n");
        return;
    }

    fputs(text.c_str(), out);
    if (text[text.size() - 1] != '\n') {
        fputc('\n', out);
    }
}

static const GeneratedRoom* FindResultGeneratedRoom(const HeadlessTurnResult& result)
{
    if (result.generated_rooms_to_add.empty()) {
        return 0;
    }
    return &result.generated_rooms_to_add[result.generated_rooms_to_add.size() - 1];
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

static float ReadFloatValue(const json& object, const char* key, float default_value)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_number()) {
        return default_value;
    }
    return object[key].get<float>();
}

static LlmGenerationConfig BuildBodyDrivenGenerationConfig(
    const LlmGenerationConfig& base_config,
    const HardState& hard_state)
{
    LlmGenerationConfig effective_config = base_config;
    const float body_driven_temperature = ComputeLlmSamplingTemperature(hard_state.body_temperature_c);
    effective_config.temperature = base_config.temperature <= 0.0f
        ? body_driven_temperature
        : base_config.temperature + (body_driven_temperature - 0.10f);
    if (effective_config.temperature > 1.50f) {
        effective_config.temperature = 1.50f;
    }
    return effective_config;
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
    const bool has_next_entropy =
        node.contains("next_spatial_entropy") &&
        node["next_spatial_entropy"].is_number_integer();
    const bool has_legacy_temperature =
        node.contains("next_datacenter_temperature_c") &&
        node["next_datacenter_temperature_c"].is_number_integer();
    delta->spatial_entropy_changed =
        (ReadBoolValue(node, "spatial_entropy_changed", false) && has_next_entropy) ||
        (ReadBoolValue(node, "temperature_changed", false) && has_legacy_temperature);
    delta->next_spatial_entropy =
        ClampSpatialEntropy(
            ReadIntValue(
                node,
                has_next_entropy ? "next_spatial_entropy" : "next_datacenter_temperature_c",
                delta->next_spatial_entropy));
    delta->external_temperature_changed =
        ReadBoolValue(node, "external_temperature_changed", false) &&
        node.contains("next_external_temperature_c") &&
        node["next_external_temperature_c"].is_number_integer();
    delta->next_external_temperature_c = ClampExternalTemperatureC(
        ReadIntValue(node, "next_external_temperature_c", delta->next_external_temperature_c));
    delta->body_temperature_changed =
        ReadBoolValue(node, "body_temperature_changed", false) &&
        node.contains("next_body_temperature_c") &&
        node["next_body_temperature_c"].is_number();
    delta->next_body_temperature_c = ClampBodyTemperatureC(
        ReadFloatValue(node, "next_body_temperature_c", delta->next_body_temperature_c));
    delta->cooling_state_changed =
        ReadBoolValue(node, "suit_state_changed", false) ||
        ReadBoolValue(node, "cooling_state_changed", false);
    ParseResourceState(
        ReadStringValue(node, node.contains("next_suit_state") ? "next_suit_state" : "next_cooling_state").c_str(),
        &delta->next_cooling_state);
    delta->water_state_changed =
        ReadBoolValue(node, "oxygen_state_changed", false) ||
        ReadBoolValue(node, "water_state_changed", false);
    ParseResourceState(
        ReadStringValue(node, node.contains("next_oxygen_state") ? "next_oxygen_state" : "next_water_state").c_str(),
        &delta->next_water_state);
    delta->power_state_changed =
        ReadBoolValue(node, "instrument_power_state_changed", false) ||
        ReadBoolValue(node, "power_state_changed", false);
    ParseResourceState(
        ReadStringValue(
            node,
            node.contains("next_instrument_power_state") ? "next_instrument_power_state" : "next_power_state").c_str(),
        &delta->next_power_state);
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
    delta->desert_state_changed =
        ReadBoolValue(node, "surface_weather_changed", false) ||
        ReadBoolValue(node, "desert_state_changed", false);
    ParseDesertState(
        ReadStringValue(node, node.contains("next_surface_weather") ? "next_surface_weather" : "next_desert_state").c_str(),
        &delta->next_desert_state);
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
    if (delta.spatial_entropy_changed) {
        state->spatial_entropy = ClampSpatialEntropy(delta.next_spatial_entropy);
    }
    if (delta.external_temperature_changed) {
        state->external_temperature_c = ClampExternalTemperatureC(delta.next_external_temperature_c);
    }
    if (delta.body_temperature_changed) {
        state->body_temperature_c = ClampBodyTemperatureC(delta.next_body_temperature_c);
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
    state->spatial_entropy = ClampSpatialEntropy(state->spatial_entropy);
    state->external_temperature_c = ClampExternalTemperatureC(state->external_temperature_c);
    state->body_temperature_c = ClampBodyTemperatureC(state->body_temperature_c);
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

static bool LooksPredominantlyEnglish(const std::string& text)
{
    const std::string lower = " " + ToLowerAsciiCopy(CollapseWhitespaceCopy(text)) + " ";
    const char* english_markers[] = {
        " the ", " you ", " your ", " and ", " with ", " into ", " from ", " this ", " that ",
        " its ", " is ", " are ", " across ", " beyond ", " through ", " remains ", " stands ",
        " fractured ", " quarry ", " survey ", " north ", " east ", " south ", " west ",
        " shelter ", " room ", " field ", " cut ", " trench ",
    };
    int score = 0;
    for (size_t index = 0; index < sizeof(english_markers) / sizeof(english_markers[0]); ++index) {
        if (lower.find(english_markers[index]) != std::string::npos) {
            ++score;
        }
    }
    return score >= 2 || lower.compare(0, 5, " the ") == 0 || lower.compare(0, 5, " you ") == 0;
}

static bool LocalizePlayerFacingText(
    const HeadlessTurnConfig& config,
    GameLanguage language,
    std::string* title,
    std::string* summary,
    std::string* narration,
    std::string* clarification,
    HeadlessTurnResult* result)
{
    json source = json::object();
    source["title"] = title ? *title : std::string();
    source["summary"] = summary ? *summary : std::string();
    source["narration"] = narration ? *narration : std::string();
    source["clarification"] = clarification ? *clarification : std::string();

    std::vector<LlmPromptMessage> messages;
    messages.push_back(LlmPromptMessage());
    messages.back().role = "system";
    messages.back().content =
        std::string("You are a strict localization pass. Translate every non-empty JSON string value into idiomatic ") +
        GameLanguageEnglishName(language) +
        ". Preserve proper names, concrete facts, directions, and gameplay affordances. Return the same four keys as valid JSON only. "
        "Never answer in English and never add explanations.";
    messages.push_back(LlmPromptMessage());
    messages.back().role = "user";
    messages.back().content =
        std::string("Translate these player-facing strings into ") + GameLanguageEnglishName(language) +
        ". Empty values must remain empty:\n" + source.dump();

    LlmGenerationConfig localization_config = config.generation_config;
    localization_config.temperature = 0.0f;
    localization_config.use_json_grammar = false;
    if (localization_config.n_predict <= 0 || localization_config.n_predict > 384) {
        localization_config.n_predict = 384;
    }

    LlmGenerationResult generation_result;
    if (!GenerateChatCompletion(localization_config, messages, &generation_result)) {
        return false;
    }
    if (result) {
        result->prompt_tokens += generation_result.prompt_tokens;
        result->generated_tokens += generation_result.generated_tokens;
        result->inference_time_ms += generation_result.inference_time_ms;
    }

    try {
        std::string extracted_json;
        const char* parse_text = generation_result.response_text.c_str();
        if (ExtractFirstJsonObject(parse_text, &extracted_json)) {
            parse_text = extracted_json.c_str();
        }
        const json root = json::parse(parse_text);
        if (!root.is_object()) {
            return false;
        }

        const std::string localized_title = ReadStringValue(root, "title");
        const std::string localized_summary = ReadStringValue(root, "summary");
        const std::string localized_narration = ReadStringValue(root, "narration");
        const std::string localized_clarification = ReadStringValue(root, "clarification");
        const std::string combined = localized_title + " " + localized_summary + " " + localized_narration + " " + localized_clarification;
        if (combined.empty() || LooksPredominantlyEnglish(combined)) {
            return false;
        }

        if (title && !localized_title.empty()) {
            *title = localized_title;
        }
        if (summary && !localized_summary.empty()) {
            *summary = localized_summary;
        }
        if (narration && !localized_narration.empty()) {
            *narration = localized_narration;
        }
        if (clarification && !localized_clarification.empty()) {
            *clarification = localized_clarification;
        }
        if (result) {
            result->player_text_localization_used = true;
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
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
    return strcmp(DescribeSpatialWorldBand(state), "open venus") == 0;
}

static bool SpatialFeelsOuterParapet(const SpatialState& state)
{
    return strcmp(DescribeSpatialWorldBand(state), "outer shelf") == 0;
}

static bool SpatialFeelsPerimeterSeam(const SpatialState& state)
{
    return strcmp(DescribeSpatialWorldBand(state), "quarry seam") == 0;
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
    const bool eryx =
        (spatial_state->location_id >= kLocationQuarryThreshold && spatial_state->location_id <= kLocationProspectShelter) ||
        combined.find("quarry") != std::string::npos ||
        combined.find("prospect") != std::string::npos ||
        combined.find("crystal") != std::string::npos ||
        combined.find("scanner station") != std::string::npos ||
        combined.find("scanner_station") != std::string::npos ||
        combined.find("industrial_service") != std::string::npos ||
        combined.find("survey") != std::string::npos ||
        combined.find("datum") != std::string::npos ||
        combined.find("venus") != std::string::npos ||
        combined.find("labyrinth threshold") != std::string::npos;
    if (eryx) {
        if (spatial_state->visible_objects.empty()) {
            if (combined.find("scanner") != std::string::npos) {
                AddUniqueString(&spatial_state->visible_objects, "crystal scanner controls");
                AddUniqueString(&spatial_state->visible_objects, "sample tray");
            } else if (combined.find("shelter") != std::string::npos) {
                AddUniqueString(&spatial_state->visible_objects, "route recorder");
                AddUniqueString(&spatial_state->visible_objects, "oxygen service manifold");
            } else if (combined.find("extraction") != std::string::npos || combined.find("drill") != std::string::npos) {
                AddUniqueString(&spatial_state->visible_objects, "rig control lever");
                AddUniqueString(&spatial_state->visible_objects, "equipment case");
            } else if (combined.find("threshold") != std::string::npos) {
                AddUniqueString(&spatial_state->visible_objects, "survey gate control");
                AddUniqueString(&spatial_state->visible_objects, "chalk datum marker");
            } else {
                AddUniqueString(&spatial_state->visible_objects, "survey beacon plate");
                AddUniqueString(&spatial_state->visible_objects, "sample case");
            }
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "scanner")) {
            AddUniqueString(&spatial_state->scene_constraints, "hero crystal scanner");
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "crystal")) {
            AddUniqueString(&spatial_state->scene_constraints, "hero crystal cluster");
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "rig") ||
            ListContainsSubstring(spatial_state->visible_objects, "drill")) {
            AddUniqueString(&spatial_state->scene_constraints, "hero extraction rig");
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "oxygen") ||
            ListContainsSubstring(spatial_state->visible_objects, "atmospheric")) {
            AddUniqueString(&spatial_state->scene_constraints, "atmospheric processor flank");
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "beacon")) {
            AddUniqueString(&spatial_state->scene_constraints, "survey beacon");
        }
        if (ListContainsSubstring(spatial_state->visible_objects, "pylon")) {
            AddUniqueString(&spatial_state->scene_constraints, "quarry pylon");
        }
        if (combined.find("labyrinth") != std::string::npos || combined.find("open datum") != std::string::npos) {
            AddUniqueString(&spatial_state->scene_constraints, "no visible wall");
        }
        if (combined.find("shelter") == std::string::npos) {
            AddUniqueString(&spatial_state->scene_constraints, "open venus horizon");
        }
        return;
    }
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

    const char* prefixes[] = {
        "go ", "move ", "walk ",
        "aller ", "va ", "marche ", "avancer ",
        "gå mot ", "ga mot ", "gå mod ", "ga mod ", "gå ", "ga ", "beveg ", "flytt ",
        "bevæg ", "bevaeg ",
        "gehe nach ", "geh nach ", "bewege dich nach ", "gehe ", "geh ", "bewege ",
        "vai verso ", "cammina verso ", "vai a ", "vai ", "andare ", "cammina ",
    };
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

static const InvisibleBarrier* FindInvisibleBarrier(
    const SessionState& session_state,
    const std::string& place_id,
    CardinalDirection direction)
{
    for (size_t index = 0; index < session_state.invisible_barriers.size(); ++index) {
        const InvisibleBarrier& barrier = session_state.invisible_barriers[index];
        if (barrier.place_id == place_id && barrier.direction == direction) {
            return &barrier;
        }
    }
    return 0;
}

static void AddOrUpdateInvisibleBarrier(
    std::vector<InvisibleBarrier>* barriers,
    const InvisibleBarrier& barrier)
{
    if (!barriers || barrier.place_id.empty() || barrier.direction == kDirectionUnknown) {
        return;
    }
    for (size_t index = 0; index < barriers->size(); ++index) {
        if ((*barriers)[index].place_id == barrier.place_id && (*barriers)[index].direction == barrier.direction) {
            (*barriers)[index] = barrier;
            return;
        }
    }
    barriers->push_back(barrier);
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
        combined.find("sky") != std::string::npos ||
        combined.find("quarry") != std::string::npos ||
        combined.find("plateau") != std::string::npos ||
        combined.find("extraction field") != std::string::npos ||
        combined.find("labyrinth threshold") != std::string::npos ||
        combined.find("venus") != std::string::npos;
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
        combined.find("ai server") != std::string::npos ||
        combined.find("accelerator") != std::string::npos ||
        combined.find("gpu") != std::string::npos ||
        combined.find("tensor") != std::string::npos ||
        combined.find("neural") != std::string::npos ||
        combined.find("compute") != std::string::npos ||
        combined.find("training") != std::string::npos;
    const bool compute_space =
        strong_ai ||
        combined.find("server") != std::string::npos ||
        combined.find("vault") != std::string::npos ||
        combined.find("backup") != std::string::npos;
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

    if (draft->spatial_state.location_archetype.empty()) {
        draft->spatial_state.location_archetype = "quarry_cut";
    }
    draft->spatial_state.interior_density = kInteriorSparse;
    AddUniqueString(&draft->spatial_state.anchors, "quarry_rim");
    AddUniqueString(&draft->spatial_state.anchors, "retaining_slab");
    AddUniqueString(&draft->spatial_state.anchors, "venus_horizon");
    AddUniqueString(&draft->spatial_state.visible_objects, "survey beacon plate");
    AddUniqueString(&draft->spatial_state.visible_objects, "sample cargo case");
    AddUniqueString(&draft->spatial_state.scene_constraints, "open venus horizon");
    AddUniqueString(&draft->spatial_state.scene_constraints, "brutalist retaining slab");
    RemoveString(&draft->spatial_state.scene_constraints, "keep corridor clear");
    AppendShortSentenceIfMissing(&draft->summary, "The quarry rim exposes a narrow strip of Venusian sky.");
    AppendShortSentenceIfMissing(&draft->arrival_narration, "Beyond the retaining slab, the quarry and sky remain open.");
}

static void ApplyOpenDesertBiasToDraft(GeneratedRoomDraft* draft)
{
    if (!draft) {
        return;
    }

    if (draft->spatial_state.location_archetype.empty()) {
        draft->spatial_state.location_archetype = "venus_plateau";
    }
    draft->spatial_state.interior_density = kInteriorSparse;
    AddUniqueString(&draft->spatial_state.anchors, "venus_plateau");
    AddUniqueString(&draft->spatial_state.anchors, "survey_line");
    AddUniqueString(&draft->spatial_state.anchors, "open_sky");
    AddUniqueString(&draft->spatial_state.visible_objects, "survey beacon");
    AddUniqueString(&draft->spatial_state.visible_objects, "quarry datum pylon");
    AddUniqueString(&draft->spatial_state.visible_objects, "sample case");
    AddUniqueString(&draft->spatial_state.scene_constraints, "open venus horizon");
    AddUniqueString(&draft->spatial_state.scene_constraints, "survey beacon line");
    AddUniqueString(&draft->spatial_state.scene_constraints, "sparse quarry terrain");
    RemoveString(&draft->spatial_state.scene_constraints, "keep corridor clear");
    AppendShortSentenceIfMissing(&draft->summary, "The extraction works fall away into a sparse Venusian survey field.");
    AppendShortSentenceIfMissing(&draft->arrival_narration, "Wind crosses the open survey line beyond the quarry rim.");
}

static void ApplyGeneratedRoomWorldBiases(GeneratedRoomDraft* draft, CardinalDirection direction)
{
    if (!draft) {
        return;
    }

    if (SpatialSuggestsAiServer(draft->spatial_state)) {
        ApplyAiServerBiasToDraft(draft);
    }
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
    draft.next_spatial_entropy = initial_session_state.hard_state.spatial_entropy;
    draft.next_external_temperature_c = initial_session_state.hard_state.external_temperature_c;
    draft.next_body_temperature_c = initial_session_state.hard_state.body_temperature_c;
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
        draft.title = desert ? "North Survey Shelf" : (exterior ? "North Quarry Ramp" : "North Service Cell");
        draft.summary = desert
            ? "Open Venusian ground extends between a survey beacon, a datum pylon and a low quarry ridge."
            : (exterior
            ? "A narrow quarry ramp runs between retaining slabs and atmospheric service hardware."
            : "A compact pressure-service cell contains oxygen hardware, cargo and a marked return passage.");
        break;
    case kDirectionEast:
        draft.title = desert ? "East Datum Field" : (exterior ? "East Extraction Apron" : "East Instrument Bay");
        draft.summary = desert
            ? "Sparse ground lies between two route datums and an apparently open horizon."
            : (exterior
            ? "An exposed extraction apron holds a rig, cargo and a split quarry marker."
            : "A lateral instrument bay contains a specimen tray, oxygen service and a survey display.");
        break;
    case kDirectionSouth:
        draft.title = desert ? "South Quarry Cut" : (exterior ? "South Retaining Ramp" : "South Pressure Trench");
        draft.summary = desert
            ? "A shallow excavation exposes a crystal trace beside a survey pylon and sample case."
            : (exterior
            ? "A sloped service ramp descends between severe concrete fins toward the quarry floor."
            : "A pressure trench links atmospheric machinery to a narrow marked access lane.");
        break;
    case kDirectionWest:
        draft.title = desert ? "West Beacon Spur" : (exterior ? "West Prospect Apron" : "West Shelter Lane");
        draft.summary = desert
            ? "A wind-scoured beacon spur ends near a pylon whose bearing conflicts with the survey slate."
            : (exterior
            ? "A prospecting apron of heavy slabs, oxygen machinery and sample cargo faces the open field."
            : "A shelter lane passes stored samples, suit service equipment and a scratched route mark.");
        break;
    default:
        draft.title = desert ? "Venus Survey Field" : (exterior ? "Quarry Service Zone" : "Prospecting Service Cell");
        draft.summary = desert
            ? "A sparse Venusian field holds only a beacon, a datum and an uncertain horizon."
            : (exterior
            ? "A sparse extraction zone sits between the quarry and human service infrastructure."
            : "A compact prospecting cell neighbors the current route.");
        break;
    }

    draft.arrival_narration = std::string("You move ") + CardinalDirectionToString(direction) + ". " + draft.summary;
    draft.spatial_state.room_title = draft.title;
    draft.spatial_state.room_summary = draft.summary;
    draft.spatial_state.location_archetype =
        desert ? "venus_plateau" : (exterior ? "extraction_field" : "industrial_service_zone");
    draft.spatial_state.anchors.push_back(desert ? "survey_line" : (exterior ? "quarry_rim" : "service_lane"));
    draft.spatial_state.anchors.push_back(desert ? "venus_horizon" : (exterior ? "retaining_slab" : "pressure_wall"));
    draft.spatial_state.anchors.push_back(desert ? "open_sky" : (exterior ? "equipment_pad" : "marked_passage"));
    if (desert) {
        draft.spatial_state.visible_objects.push_back("survey beacon");
        draft.spatial_state.visible_objects.push_back("quarry datum pylon");
        draft.spatial_state.visible_objects.push_back("sample case");
        draft.spatial_state.scene_constraints.push_back("open venus horizon");
        draft.spatial_state.scene_constraints.push_back("survey beacon line");
    } else if (exterior) {
        draft.spatial_state.visible_objects.push_back("atmospheric processor");
        draft.spatial_state.visible_objects.push_back("prospecting cargo crate");
        draft.spatial_state.visible_objects.push_back("quarry pylon");
        draft.spatial_state.scene_constraints.push_back("open venus horizon");
        draft.spatial_state.scene_constraints.push_back("brutalist retaining slab");
    } else {
        draft.spatial_state.visible_objects.push_back("oxygen service manifold");
        draft.spatial_state.visible_objects.push_back("sample crate");
        draft.spatial_state.visible_objects.push_back("survey map panel");
        draft.spatial_state.scene_constraints.push_back("keep marked passage clear");
        draft.spatial_state.scene_constraints.push_back("atmospheric processor flank");
    }
    ApplyGeneratedRoomWorldBiases(&draft, direction);
    if (initial_session_state.language != kGameLanguageEnglish) {
        const char* north_title = PlayerText(
            initial_session_state.language, "Northern Relay", "Relais septentrional", "Nordlig relé",
            "Nordligt relæ", "Nördliche Relaisstation", "Ripetitore settentrionale");
        const char* east_title = PlayerText(
            initial_session_state.language, "Eastern Marker Field", "Champ de repères oriental", "Østre merkefelt",
            "Østligt markeringsfelt", "Östliches Markierungsfeld", "Campo di riferimenti orientale");
        const char* south_title = PlayerText(
            initial_session_state.language, "Southern Cut", "Entaille méridionale", "Sørlig skjæring",
            "Sydligt snit", "Südlicher Einschnitt", "Taglio meridionale");
        const char* west_title = PlayerText(
            initial_session_state.language, "Western Beacon Spur", "Éperon de la balise occidentale", "Vestlig signalrygg",
            "Vestlig signalryg", "Westlicher Signalsporn", "Sperone del faro occidentale");
        switch (direction) {
        case kDirectionNorth: draft.title = north_title; break;
        case kDirectionEast: draft.title = east_title; break;
        case kDirectionSouth: draft.title = south_title; break;
        case kDirectionWest: draft.title = west_title; break;
        default:
            draft.title = PlayerText(
                initial_session_state.language, "Venusian Survey Field", "Champ d'exploration vénusien",
                "Venusiansk prospekteringsfelt", "Venusiansk prospekteringsfelt",
                "Venusisches Prospektionsfeld", "Campo di esplorazione venusiano");
            break;
        }
        draft.summary = exterior
            ? PlayerText(
                initial_session_state.language,
                "Hostile Venusian ground separates the survey markers from the quarry masses.",
                "Un terrain vénusien hostile sépare les repères de prospection et les masses brutales de la carrière.",
                "Fiendtlig venusisk terreng skiller peilemerkene fra steinbruddets massive former.",
                "Fjendtligt venusisk terræn adskiller pejlemærkerne fra stenbruddets massive former.",
                "Feindliches venusisches Gelände trennt die Vermessungsmarken von den massiven Formen des Steinbruchs.",
                "Un terreno venusiano ostile separa i riferimenti di prospezione dalle masse della cava.")
            : PlayerText(
                initial_session_state.language,
                "A pressurized service cell extends the survey route.",
                "Une cellule de service pressurisée prolonge la route de prospection.",
                "En trykksatt servicecelle forlenger prospekteringsruten.",
                "En tryksat servicecelle forlænger prospekteringsruten.",
                "Eine druckbeaufschlagte Servicezelle verlängert die Prospektionsroute.",
                "Una cella di servizio pressurizzata prolunga il percorso di prospezione.");
        draft.arrival_narration = std::string(PlayerText(
            initial_session_state.language, "You reach ", "Vous atteignez ", "Du når ", "Du når ",
            "Du erreichst ", "Raggiungi ")) + draft.title + ". " + draft.summary;
        draft.spatial_state.room_title = draft.title;
        draft.spatial_state.room_summary = draft.summary;
    }
    return draft;
}

static std::string BuildFallbackGeneratedRoomSceneText(const SpatialState& spatial_state)
{
    const bool plateau = SpatialFeelsOpenDesert(spatial_state);
    const bool exterior = SpatialFeelsExterior(spatial_state);
    const bool wants_gate =
        ListContainsSubstring(spatial_state.visible_objects, "gate") ||
        ListContainsSubstring(spatial_state.scene_constraints, "gate") ||
        ListContainsSubstring(spatial_state.scene_constraints, "door");
    const bool wants_crystal =
        ListContainsSubstring(spatial_state.visible_objects, "crystal") ||
        ListContainsSubstring(spatial_state.scene_constraints, "crystal");
    const bool wants_scanner =
        ListContainsSubstring(spatial_state.visible_objects, "scanner") ||
        ListContainsSubstring(spatial_state.scene_constraints, "scanner") ||
        ListContainsSubstring(spatial_state.scene_constraints, "instrument");
    std::string text;
    text += "room \"generated room\"\n";

    if (plateau) {
        text += "camera eye(0.0,1.82,-9.6) target(0.0,1.18,9.8) up(0.0,1.0,0.0) fov(48.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(36.0) cone(12.0,28.0) intensity(60.0)\n";
        text += "sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(97)\n";
        text += "plane \"ground\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(120.0,170.0) gray(0.13)\n";
        text += "box \"quarry_ridge_left\" pos(-7.8,0.42,11.0) size(7.0,0.84,4.2) gray(0.14)\n";
        text += "box \"quarry_ridge_right\" pos(8.6,0.36,14.0) size(8.5,0.72,3.4) gray(0.15)\n";
        text += "prefab_survey_beacon \"route_beacon_near\" pos(-3.6,1.15,3.8) size(0.9,2.3,0.9) gray(0.27) detail(0.42)\n";
        text += "prefab_survey_beacon \"route_beacon_far\" pos(2.2,0.90,9.4) size(0.7,1.8,0.7) gray(0.24) detail(0.40)\n";
        text += "prefab_quarry_pylon \"datum_pylon\" pos(5.2,1.35,5.8) size(1.1,2.7,1.1) gray(0.23) detail(0.32)\n";
        text += "prefab_crate \"sample_case\" pos(0.6,0.55,3.8) size(1.5,1.0,1.2) gray(0.20) detail(0.30)\n";
    } else if (exterior) {
        text += "camera eye(0.0,1.82,-8.4) target(0.0,1.20,8.2) up(0.0,1.0,0.0) fov(46.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(34.0) cone(12.0,28.0) intensity(72.0)\n";
        text += "sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(91)\n";
        text += "plane \"ground\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(90.0,150.0) gray(0.13)\n";
        text += "box \"retaining_mass\" pos(-5.2,1.8,8.8) size(5.4,3.6,2.2) gray(0.18)\n";
        text += "box \"cantilever\" pos(-2.8,3.35,6.8) size(9.8,1.0,4.8) gray(0.23)\n";
        text += "box \"work_slab\" pos(2.0,0.18,2.8) size(8.8,0.36,8.0) gray(0.21)\n";
        if (wants_gate) {
            text += "prefab_gate \"aux_gate\" pos(0.0,1.4,7.2) size(4.6,2.8,0.5) gray(0.28) detail(0.40)\n";
        }
        text += "prefab_extraction_rig \"field_rig\" pos(2.5,1.35,4.6) size(2.4,2.7,2.4) gray(0.22) detail(0.38)\n";
        text += "prefab_atmospheric_processor \"air_processor\" pos(-4.6,1.25,1.6) size(1.8,2.5,1.8) gray(0.24) detail(0.36)\n";
        text += "prefab_crate \"sample_case\" pos(4.6,0.55,-0.8) size(1.8,1.1,1.5) gray(0.20) detail(0.31)\n";
        text += "prefab_survey_beacon \"route_beacon\" pos(-1.0,1.05,-1.2) size(0.8,2.1,0.8) gray(0.29) detail(0.42)\n";
    } else {
        text += "camera eye(0.0,1.78,-7.8) target(0.0,1.20,8.4) up(0.0,1.0,0.0) fov(46.0)\n";
        text += "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(30.0) cone(12.0,28.0) intensity(78.0)\n";
        text += "plane \"floor\" pos(0.0,0.0,0.0) normal(0.0,1.0,0.0) size(14.0,24.0) gray(0.12)\n";
        text += "box \"pressure_wall\" pos(-6.4,1.9,1.5) size(0.8,3.8,21.0) gray(0.18)\n";
        text += "box \"heavy_roof\" pos(-1.7,3.5,4.4) size(9.8,0.9,8.8) gray(0.21)\n";
        text += "box \"open_side_pier\" pos(4.8,1.7,6.8) size(1.2,3.4,1.2) gray(0.23)\n";
        if (wants_scanner) {
            text += "prefab_crystal_scanner \"survey_scanner\" pos(0.3,1.25,3.8) size(2.1,2.5,1.8) gray(0.23) detail(0.43)\n";
        }
        if (wants_crystal) {
            text += "prefab_crystal_cluster \"reference_crystal\" pos(3.6,0.95,6.4) size(1.6,1.9,1.6) gray(0.28) detail(0.46)\n";
        }
        text += "prefab_atmospheric_processor \"suit_service\" pos(-3.8,1.0,-1.8) size(1.5,2.0,1.5) gray(0.28) detail(0.38)\n";
        text += "prefab_crate \"sample_case\" pos(2.7,0.55,-1.2) size(1.7,1.0,1.4) gray(0.20) detail(0.30)\n";
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
    CaptureSceneDebugArtifacts(
        refreshed_room.spatial_state,
        &refreshed_room.scene_text,
        &result->rendered_scene_text,
        &result->rendered_scene_debug_text);
    result->rendered_scene = refreshed_scene;
    result->used_candidate_scene_for_render = false;
    return true;
}

static const char* DescribeDirection(CardinalDirection direction, GameLanguage language)
{
    switch (language) {
    case kGameLanguageFrench:
        switch (direction) {
        case kDirectionNorth: return "le nord";
        case kDirectionEast: return "l'est";
        case kDirectionSouth: return "le sud";
        case kDirectionWest: return "l'ouest";
        default: return "direction inconnue";
        }
    case kGameLanguageNorwegian:
        switch (direction) {
        case kDirectionNorth: return "nordover";
        case kDirectionEast: return "østover";
        case kDirectionSouth: return "sørover";
        case kDirectionWest: return "vestover";
        default: return "i ukjent retning";
        }
    case kGameLanguageDanish:
        switch (direction) {
        case kDirectionNorth: return "mod nord";
        case kDirectionEast: return "mod øst";
        case kDirectionSouth: return "mod syd";
        case kDirectionWest: return "mod vest";
        default: return "i ukendt retning";
        }
    case kGameLanguageGerman:
        switch (direction) {
        case kDirectionNorth: return "nach Norden";
        case kDirectionEast: return "nach Osten";
        case kDirectionSouth: return "nach Süden";
        case kDirectionWest: return "nach Westen";
        default: return "in unbekannte Richtung";
        }
    case kGameLanguageItalian:
        switch (direction) {
        case kDirectionNorth: return "verso nord";
        case kDirectionEast: return "verso est";
        case kDirectionSouth: return "verso sud";
        case kDirectionWest: return "verso ovest";
        default: return "in una direzione sconosciuta";
        }
    case kGameLanguageEnglish:
    default:
        return CardinalDirectionToString(direction);
    }
}

static std::string BuildBlockedTraversalNarration(
    const SpatialState& spatial_state,
    CardinalDirection direction,
    GameLanguage language)
{
    std::string narration = PlayerText(
        language, "The way ", "Le passage vers ", "Veien ", "Vejen ", "Der Weg ", "Il passaggio ");
    narration += DescribeDirection(direction, language);
    narration += PlayerText(
        language, " is blocked.", " est bloqué.", " er blokkert.", " er blokeret.", " ist versperrt.", " è bloccato.");
    if (language == kGameLanguageEnglish && !spatial_state.room_summary.empty()) {
        narration += " ";
        narration += spatial_state.room_summary;
    }
    return ConstrainNarrationText(narration, 220, 2);
}

static std::string BuildInvisibleBarrierNarration(const InvisibleBarrier& barrier, GameLanguage language)
{
    std::string narration;
    narration = PlayerText(
        language, "You advance ", "Vous avancez vers ", "Du går ", "Du går ", "Du gehst ", "Avanzi ");
    narration += DescribeDirection(barrier.direction, language);
    narration += PlayerText(
        language,
        " across apparently open ground. Your probe and suit strike a smooth plane where the viewport shows only sky.",
        " sur un terrain apparemment libre. Votre sonde et votre scaphandre heurtent un plan lisse là où la visière ne montre que le ciel.",
        " over tilsynelatende åpent terreng. Sonden og romdrakten treffer en glatt flate der visiret bare viser himmel.",
        " over tilsyneladende åbent terræn. Sonden og rumdragten rammer en glat flade, hvor visiret kun viser himmel.",
        " über scheinbar freies Gelände. Deine Sonde und dein Raumanzug stoßen gegen eine glatte Fläche, wo das Visier nur Himmel zeigt.",
        " su un terreno apparentemente libero. La sonda e la tuta urtano una superficie liscia dove la visiera mostra soltanto il cielo.");
    if (language == kGameLanguageEnglish && !barrier.evidence.empty()) {
        narration += " ";
        narration += barrier.evidence;
    }
    return ConstrainNarrationText(narration, 300, 3);
}

static std::string BuildTraversalNarrationForKnownPlace(const SessionState& session_state, const std::string& place_id, CardinalDirection direction)
{
    const bool localized = session_state.language != kGameLanguageEnglish;
    std::string narration = PlayerText(
        session_state.language, "You go ", "Vous allez vers ", "Du går ", "Du går ", "Du gehst ", "Vai ");
    narration += DescribeDirection(direction, session_state.language);
    narration += PlayerText(
        session_state.language, " and enter ", " et atteignez ", " og kommer til ", " og når ",
        " und erreichst ", " e raggiungi ");
    narration += DescribePlaceLabel(session_state, place_id);
    narration += ".";

    const GeneratedRoom* room = FindGeneratedRoomById(session_state, place_id);
    if (!localized && room && !room->spatial_state.room_summary.empty()) {
        narration += " ";
        narration += room->spatial_state.room_summary;
    } else {
        LocationId location_id = kLocationUnknown;
        SpatialState canonical_state;
        if (!localized && ParseCanonicalPlaceId(place_id, &location_id) &&
            BuildCanonicalSpatialState(location_id, &canonical_state) &&
            !canonical_state.room_summary.empty()) {
            narration += " ";
            narration += canonical_state.room_summary;
        }
    }
    return ConstrainNarrationText(narration, 240, 3);
}

static void AskLlmForThermalUpdate(
    const HeadlessTurnConfig& config,
    const SessionState& initial_session_state,
    const char* player_command,
    const SpatialState& target_spatial_state,
    HardState* hard_state,
    HeadlessTurnResult* result)
{
    if (!hard_state || !result) {
        return;
    }

    std::string prompt;
    prompt += "Assess only the suit's thermal response to this completed action on Venus. Internal mechanics and JSON keys remain English. Return exactly one JSON object and no markdown.\n";
    prompt += "The suit buffers the body from the exterior. External and body temperatures are correlated but lagged. Open exposure, effort, time, or suit strain may heat the body; shelter or atmospheric machinery may cool it. The values may also remain unchanged. Keep body changes small, normally 0.0 to 0.4 C for one action.\n";
    prompt += "Schema: {\"external_temperature_changed\":boolean,\"next_external_temperature_c\":integer,\"body_temperature_changed\":boolean,\"next_body_temperature_c\":number}\n";
    prompt += "current_external_temperature_c: " + std::to_string(initial_session_state.hard_state.external_temperature_c) + "\n";
    prompt += "current_body_temperature_c: " + std::to_string(initial_session_state.hard_state.body_temperature_c) + "\n";
    prompt += "suit_state: ";
    prompt += ResourceStateToString(initial_session_state.hard_state.cooling_state);
    prompt += "\naction: ";
    prompt += player_command ? player_command : "move";
    prompt += "\ntarget_spatial_brief:\n";
    prompt += BuildSpatialBriefText(target_spatial_state);

    std::vector<LlmPromptMessage> messages;
    messages.push_back(LlmPromptMessage());
    messages.back().role = "system";
    messages.back().content = "You are an internal suit thermal-state controller. Return JSON only.";
    messages.push_back(LlmPromptMessage());
    messages.back().role = "user";
    messages.back().content = prompt;

    LlmGenerationConfig thermal_config = config.generation_config;
    thermal_config.use_json_grammar = false;
    if (thermal_config.n_predict <= 0 || thermal_config.n_predict > 160) {
        thermal_config.n_predict = 160;
    }

    LlmGenerationResult generation_result;
    if (!GenerateChatCompletion(thermal_config, messages, &generation_result)) {
        return;
    }

    result->thermal_prompt_text = generation_result.prompt_text;
    result->thermal_response_text = generation_result.response_text;
    result->prompt_tokens += generation_result.prompt_tokens;
    result->generated_tokens += generation_result.generated_tokens;
    result->inference_time_ms += generation_result.inference_time_ms;

    try {
        std::string json_text;
        const char* parse_text = generation_result.response_text.c_str();
        if (ExtractFirstJsonObject(parse_text, &json_text)) {
            parse_text = json_text.c_str();
        }
        const json root = json::parse(parse_text);
        if (!root.is_object()) {
            return;
        }
        if (ReadBoolValue(root, "external_temperature_changed", false)) {
            hard_state->external_temperature_c = ClampExternalTemperatureC(
                ReadIntValue(root, "next_external_temperature_c", hard_state->external_temperature_c));
        }
        if (ReadBoolValue(root, "body_temperature_changed", false)) {
            hard_state->body_temperature_c = ClampBodyTemperatureC(
                ReadFloatValue(root, "next_body_temperature_c", hard_state->body_temperature_c));
        }
        result->thermal_update_used = true;
    } catch (const std::exception&) {
        return;
    }
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
            ? "A sparse survey pocket between the Venusian quarry and a marked human datum."
            : "A compact pressure-service cell branching away from the prospecting route.";
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
    draft->next_spatial_entropy = ClampSpatialEntropy(draft->next_spatial_entropy);
    draft->next_external_temperature_c = ClampExternalTemperatureC(draft->next_external_temperature_c);
    draft->next_body_temperature_c = ClampBodyTemperatureC(draft->next_body_temperature_c);

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
    ParseDesertState(
        ReadStringValue(node, node.contains("surface_weather") ? "surface_weather" : "desert_state").c_str(),
        &spatial_state->desert_state);
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
    const HardState& current_hard_state,
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
    draft->next_spatial_entropy = current_hard_state.spatial_entropy;
    draft->next_external_temperature_c = current_hard_state.external_temperature_c;
    draft->next_body_temperature_c = current_hard_state.body_temperature_c;
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
        const bool has_next_entropy =
            root.contains("next_spatial_entropy") &&
            root["next_spatial_entropy"].is_number_integer();
        const bool has_legacy_temperature =
            root.contains("next_datacenter_temperature_c") &&
            root["next_datacenter_temperature_c"].is_number_integer();
        draft->next_spatial_entropy = ClampSpatialEntropy(
            ReadIntValue(
                root,
                has_next_entropy ? "next_spatial_entropy" : (has_legacy_temperature ? "next_datacenter_temperature_c" : "next_spatial_entropy"),
                draft->next_spatial_entropy));
        draft->next_external_temperature_c = ClampExternalTemperatureC(
            ReadIntValue(root, "next_external_temperature_c", draft->next_external_temperature_c));
        draft->next_body_temperature_c = ClampBodyTemperatureC(
            ReadFloatValue(root, "next_body_temperature_c", draft->next_body_temperature_c));
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
        for (size_t barrier_index = 0; barrier_index < session_state.invisible_barriers.size(); ++barrier_index) {
            const InvisibleBarrier& barrier = session_state.invisible_barriers[barrier_index];
            if (barrier.place_id == place_id && barrier.discovered && !barrier.evidence.empty()) {
                AddUniqueString(&spatial_state->spatial_anomalies, barrier.evidence);
            }
        }
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

static bool IsEryxLocation(LocationId location_id)
{
    return location_id >= kLocationQuarryThreshold && location_id <= kLocationProspectShelter;
}

static void ApplyEryxSpatialEntropy(LocationId location_id, HardState* hard_state)
{
    if (!hard_state) {
        return;
    }

    int minimum_entropy = kDefaultSpatialEntropy;
    switch (location_id) {
    case kLocationExtractionField:
        minimum_entropy = 12;
        break;
    case kLocationCrystalCut:
        minimum_entropy = 18;
        break;
    case kLocationScannerStation:
        minimum_entropy = 26;
        break;
    case kLocationSurveyPlateau:
        minimum_entropy = 34;
        break;
    case kLocationLabyrinthThreshold:
        minimum_entropy = 48;
        break;
    case kLocationProspectShelter:
        minimum_entropy = 55;
        break;
    default:
        break;
    }
    if (hard_state->spatial_entropy < minimum_entropy) {
        hard_state->spatial_entropy = minimum_entropy;
    }
}

static void AddAuthoredEryxTopology(SessionState* session_state)
{
    if (!session_state) {
        return;
    }

    const std::string threshold = BuildCanonicalPlaceId(kLocationQuarryThreshold);
    const std::string extraction = BuildCanonicalPlaceId(kLocationExtractionField);
    const std::string crystal = BuildCanonicalPlaceId(kLocationCrystalCut);
    const std::string scanner = BuildCanonicalPlaceId(kLocationScannerStation);
    const std::string plateau = BuildCanonicalPlaceId(kLocationSurveyPlateau);
    const std::string labyrinth = BuildCanonicalPlaceId(kLocationLabyrinthThreshold);
    const std::string shelter = BuildCanonicalPlaceId(kLocationProspectShelter);

    AddRoomLinkUnique(&session_state->room_links, threshold, kDirectionNorth, extraction);
    AddRoomLinkUnique(&session_state->room_links, extraction, kDirectionSouth, threshold);
    AddRoomLinkUnique(&session_state->room_links, extraction, kDirectionNorth, crystal);
    AddRoomLinkUnique(&session_state->room_links, crystal, kDirectionSouth, extraction);
    AddRoomLinkUnique(&session_state->room_links, crystal, kDirectionEast, scanner);
    AddRoomLinkUnique(&session_state->room_links, scanner, kDirectionWest, crystal);
    AddRoomLinkUnique(&session_state->room_links, scanner, kDirectionNorth, plateau);
    AddRoomLinkUnique(&session_state->room_links, plateau, kDirectionSouth, scanner);
    AddRoomLinkUnique(&session_state->room_links, plateau, kDirectionEast, labyrinth);
    AddRoomLinkUnique(&session_state->room_links, labyrinth, kDirectionWest, plateau);
    AddRoomLinkUnique(&session_state->room_links, labyrinth, kDirectionEast, shelter);

    // The shelter's west return is intentionally non-reciprocal: it bypasses the
    // open datum and returns to the locally recognizable scanner station.
    AddRoomLinkUnique(&session_state->room_links, shelter, kDirectionWest, scanner);

    InvisibleBarrier northern_plane;
    northern_plane.place_id = labyrinth;
    northern_plane.direction = kDirectionNorth;
    northern_plane.evidence =
        "The contact line runs east-west; the plateau remains reachable to the west and Vey's shelter beacon remains visible to the east.";
    AddOrUpdateInvisibleBarrier(&session_state->invisible_barriers, northern_plane);
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
    if (IsEryxLocation(initial_location_id)) {
        AddAuthoredEryxTopology(session_state);
        ApplyEryxSpatialEntropy(initial_location_id, &session_state->hard_state);
    }
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
    for (size_t index = 0; index < turn_result.invisible_barriers_to_update.size(); ++index) {
        AddOrUpdateInvisibleBarrier(
            &session_state->invisible_barriers,
            turn_result.invisible_barriers_to_update[index]);
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
    HeadlessTurnConfig body_driven_config = config;
    body_driven_config.generation_config = BuildBodyDrivenGenerationConfig(
        config.generation_config,
        result->initial_hard_state);
    result->effective_llm_temperature = body_driven_config.generation_config.temperature;

    CardinalDirection traversal_direction = kDirectionUnknown;
    if (TryParseTraversalCommand(player_command, &traversal_direction)) {
        result->traversal_requested = true;
        result->traversal_direction = traversal_direction;

        const InvisibleBarrier* invisible_barrier = FindInvisibleBarrier(
            initial_session_state,
            result->initial_place_id,
            traversal_direction);
        if (invisible_barrier) {
            InvisibleBarrier discovered_barrier = *invisible_barrier;
            discovered_barrier.discovered = true;
            result->invisible_barriers_to_update.push_back(discovered_barrier);
            result->invisible_barrier_contact = true;
            result->turn_result.intent =
                std::string("move_") + CardinalDirectionToString(traversal_direction) + "_invisible_barrier";
            result->turn_result.narration = BuildInvisibleBarrierNarration(discovered_barrier, initial_session_state.language);
            result->turn_result.clarification = PlayerText(
                initial_session_state.language,
                "Traversal was recognized; an invisible barrier, not a parser failure or visible obstacle, refused the move.",
                "Le déplacement a été reconnu : une barrière invisible, et non un échec de compréhension ou un obstacle visible, a refusé le passage.",
                "Forflytningen ble gjenkjent: En usynlig barriere, ikke en tolkningsfeil eller et synlig hinder, stanset deg.",
                "Bevægelsen blev genkendt: En usynlig barriere, ikke en fortolkningsfejl eller en synlig hindring, standsede dig.",
                "Die Bewegung wurde erkannt: Eine unsichtbare Barriere, nicht ein Verständnisfehler oder ein sichtbares Hindernis, hielt dich auf.",
                "Il movimento è stato riconosciuto: una barriera invisibile, non un errore di comprensione o un ostacolo visibile, ha impedito il passaggio.");
            result->updated_soft_state.rolling_summary = result->turn_result.narration;
            AddUniqueString(&result->updated_spatial_state.spatial_anomalies, discovered_barrier.evidence);
            AskLlmForThermalUpdate(
                body_driven_config,
                initial_session_state,
                player_command,
                result->updated_spatial_state,
                &result->updated_hard_state,
                result);
            ++result->updated_hard_state.turn_number;
            CaptureSceneDebugArtifacts(
                result->updated_spatial_state,
                0,
                &result->rendered_scene_text,
                &result->rendered_scene_debug_text);
            if (!LoadSceneForPlace(initial_session_state, result->updated_place_id, &result->rendered_scene, error_buffer, error_buffer_size)) {
                return false;
            }
            return true;
        }

        if (SpatialStateBlocksDirection(result->initial_spatial_state, traversal_direction)) {
            result->turn_result.intent = std::string("move_") + CardinalDirectionToString(traversal_direction) + "_blocked";
            result->turn_result.narration = BuildBlockedTraversalNarration(
                result->initial_spatial_state,
                traversal_direction,
                initial_session_state.language);
            result->turn_result.clarification = PlayerText(
                initial_session_state.language,
                "No new room was generated because the current spatial brief marks that exit as blocked.",
                "Aucun nouveau lieu n'a été généré car cette sortie est indiquée comme bloquée.",
                "Ingen ny plass ble generert fordi den romlige beskrivelsen markerer denne utgangen som blokkert.",
                "Intet nyt sted blev genereret, fordi den rumlige beskrivelse markerer denne udgang som blokeret.",
                "Es wurde kein neuer Ort erzeugt, weil die räumliche Beschreibung diesen Ausgang als versperrt markiert.",
                "Non è stato generato alcun nuovo luogo perché la descrizione spaziale indica questa uscita come bloccata.");
            result->updated_soft_state.rolling_summary = result->turn_result.narration;
            AskLlmForThermalUpdate(
                body_driven_config,
                initial_session_state,
                player_command,
                result->updated_spatial_state,
                &result->updated_hard_state,
                result);
            ++result->updated_hard_state.turn_number;
            CaptureSceneDebugArtifacts(
                result->updated_spatial_state,
                0,
                &result->rendered_scene_text,
                &result->rendered_scene_debug_text);
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
            ApplyEryxSpatialEntropy(result->updated_hard_state.current_location_id, &result->updated_hard_state);
            AskLlmForThermalUpdate(
                body_driven_config,
                initial_session_state,
                player_command,
                result->updated_spatial_state,
                &result->updated_hard_state,
                result);
            result->updated_soft_state.rolling_summary = result->turn_result.narration;
            ++result->updated_hard_state.move_count;
            ++result->updated_hard_state.turn_number;
            const GeneratedRoom* linked_room = FindGeneratedRoomById(initial_session_state, linked_place_id);
            CaptureSceneDebugArtifacts(
                result->updated_spatial_state,
                linked_room ? &linked_room->scene_text : 0,
                &result->rendered_scene_text,
                &result->rendered_scene_debug_text);
            if (!LoadSceneForPlace(initial_session_state, linked_place_id, &result->rendered_scene, error_buffer, error_buffer_size)) {
                return false;
            }
            return true;
        }

        const SpatialState prospective_spatial_state = BuildProspectiveSpatialStateForTraversal(
            initial_session_state,
            result->initial_place_id,
            traversal_direction);
        result->prospective_spatial_state = prospective_spatial_state;
        result->prospective_spatial_state_known = true;

        const std::string room_prompt = BuildGeneratedRoomPrompt(
            initial_session_state.language,
            result->initial_hard_state,
            result->initial_soft_state,
            result->initial_spatial_state,
            prospective_spatial_state,
            &initial_session_state.history,
            traversal_direction);
        result->request_text = room_prompt;

        std::vector<LlmPromptMessage> room_messages;
        room_messages.push_back(LlmPromptMessage());
        room_messages.back().role = "system";
        room_messages.back().content =
            std::string("You invent one new neighboring room for a local interactive-fiction prototype. Return valid JSON only. ") +
            "MANDATORY LANGUAGE RULE: title, summary, and arrival_narration must be written entirely in idiomatic " +
            GameLanguageEnglishName(initial_session_state.language) +
            ". JSON keys, IDs, object tokens, anchors, scene constraints, and all internal mechanics remain English. "
            "Do not use markdown fences.";
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
                body_driven_config.generation_config,
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
                    result->initial_hard_state,
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
                result->turn_result.clarification = initial_session_state.language == kGameLanguageEnglish
                    ? std::string("Generated room metadata fallback was used: ") + metadata_error
                    : PlayerText(
                        initial_session_state.language, "Generated room metadata fallback was used.",
                        "Les métadonnées du lieu ont été remplacées par une solution de secours.",
                        "Reserveløsningen ble brukt for metadataene til stedet.",
                        "Reserveløsningen blev brugt til stedets metadata.",
                        "Für die Ortsmetadaten wurde die Ersatzlösung verwendet.",
                        "Per i metadati del luogo è stata usata la soluzione di riserva.");
            }
        } else {
            draft = MakeFallbackGeneratedRoomDraft(
                initial_session_state,
                traversal_direction,
                prospective_spatial_state);
            used_room_metadata_fallback = true;
            result->used_turn_fallback = true;
            result->turn_result.clarification = initial_session_state.language == kGameLanguageEnglish
                ? std::string("Generated room metadata fallback was used after LLM failure: ") +
                    (room_generation_result.error_message.empty() ? "unknown error" : room_generation_result.error_message)
                : PlayerText(
                    initial_session_state.language, "The fallback built the location after a model failure.",
                    "Le lieu a été construit par la solution de secours après un échec du modèle.",
                    "Reserveløsningen bygde stedet etter en modellfeil.",
                    "Reserveløsningen byggede stedet efter en modelfejl.",
                    "Nach einem Modellfehler wurde der Ort durch die Ersatzlösung aufgebaut.",
                    "Il luogo è stato costruito dalla soluzione di riserva dopo un errore del modello.");
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
        if (initial_session_state.language != kGameLanguageEnglish &&
            LooksPredominantlyEnglish(draft.title + " " + draft.summary + " " + draft.arrival_narration)) {
            if (!LocalizePlayerFacingText(
                    body_driven_config,
                    initial_session_state.language,
                    &draft.title,
                    &draft.summary,
                    &draft.arrival_narration,
                    0,
                    result)) {
                draft.title = PlayerText(
                    initial_session_state.language, "Fractured Survey Sector", "Secteur de prospection fracturé",
                    "Brutt prospekteringssektor", "Brudt prospekteringssektor", "Gebrochener Prospektionssektor",
                    "Settore di prospezione fratturato");
                draft.summary = PlayerText(
                    initial_session_state.language,
                    "A new Venusian sector extends the route between survey markers and quarry masses.",
                    "Un nouveau secteur vénusien prolonge la route entre les repères de prospection et les masses de la carrière.",
                    "En ny venusisk sektor forlenger ruten mellom peilemerkene og steinbruddets massive former.",
                    "En ny venusisk sektor forlænger ruten mellem pejlemærkerne og stenbruddets massive former.",
                    "Ein neuer venusischer Sektor verlängert die Route zwischen Vermessungsmarken und den massiven Formen des Steinbruchs.",
                    "Un nuovo settore venusiano prolunga il percorso tra i riferimenti di prospezione e le masse della cava.");
                draft.arrival_narration = PlayerText(
                    initial_session_state.language,
                    "You reach a survey sector whose markers no longer quite agree.",
                    "Vous atteignez un secteur de prospection dont les repères ne concordent plus tout à fait.",
                    "Du når en prospekteringssektor der peilemerkene ikke lenger stemmer helt overens.",
                    "Du når en prospekteringssektor, hvor pejlemærkerne ikke længere stemmer helt overens.",
                    "Du erreichst einen Prospektionssektor, dessen Markierungen nicht mehr ganz übereinstimmen.",
                    "Raggiungi un settore di prospezione i cui riferimenti non coincidono più del tutto.");
            }
            draft.title = ConstrainNarrationText(draft.title, 72, 1);
            draft.summary = ConstrainNarrationText(draft.summary, 160, 2);
            draft.arrival_narration = ConstrainNarrationText(draft.arrival_narration, 260, 3);
            draft.spatial_state.room_title = draft.title;
            draft.spatial_state.room_summary = draft.summary;
        }

        std::string generated_scene_text;
        Scene generated_scene;
        std::string generated_scene_source;
        if (!BuildGeneratedRoomSceneProgram(
                body_driven_config,
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
        CaptureSceneDebugArtifacts(
            generated_room.spatial_state,
            &generated_scene_text,
            &result->rendered_scene_text,
            &result->rendered_scene_debug_text);
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
        result->updated_hard_state.spatial_entropy = draft.next_spatial_entropy;
        result->updated_hard_state.external_temperature_c = draft.next_external_temperature_c;
        result->updated_hard_state.body_temperature_c = draft.next_body_temperature_c;
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
            result->turn_result.clarification = PlayerText(
                initial_session_state.language,
                "Generated room metadata fallback was used.",
                "Les métadonnées du lieu proviennent de la solution de secours.",
                "Reserveløsningen ble brukt for metadataene til stedet.",
                "Reserveløsningen blev brugt til stedets metadata.",
                "Für die Ortsmetadaten wurde die Ersatzlösung verwendet.",
                "Per i metadati del luogo è stata usata la soluzione di riserva.");
        }
        if (used_room_scene_fallback) {
            if (!result->turn_result.clarification.empty()) {
                result->turn_result.clarification.append(" ");
            }
            result->turn_result.clarification.append(
                PlayerText(
                    initial_session_state.language,
                    "Generated room scene fallback was used.",
                    "La scène du lieu provient de la solution de secours.",
                    "Reserveløsningen ble brukt for scenen til stedet.",
                    "Reserveløsningen blev brugt til stedets scene.",
                    "Für die Ortsszene wurde die Ersatzlösung verwendet.",
                    "Per la scena del luogo è stata usata la soluzione di riserva."));
        }
        result->rendered_scene = generated_scene;
        return true;
    }

    const std::string user_prompt = BuildTurnPrompt(
        initial_session_state.language,
        result->initial_hard_state,
        result->initial_soft_state,
        result->initial_spatial_state,
        &initial_session_state.history,
        player_command,
        false);
    result->request_text = user_prompt;

    std::vector<LlmPromptMessage> messages;
        messages.push_back(LlmPromptMessage());
        messages.back().role = "system";
    messages.back().content =
        std::string("You are a deterministic interactive-fiction turn engine. Return valid JSON only. ") +
        "MANDATORY LANGUAGE RULE: narration and clarification must be written entirely in idiomatic " +
        GameLanguageEnglishName(initial_session_state.language) +
        ". JSON keys, IDs, intent labels, object tokens, and internal mechanics remain English. "
        "Do not use markdown fences or write outside the JSON object.";
    messages.push_back(LlmPromptMessage());
    messages.back().role = "user";
    messages.back().content = user_prompt;

    LlmGenerationResult generation_result;
    StreamForwarder turn_stream_forwarder;
    turn_stream_forwarder.callback = config.stream_callback;
    turn_stream_forwarder.phase = kHeadlessTurnStreamPrimaryResponse;
    turn_stream_forwarder.user_data = config.stream_user_data;
    if (!GenerateChatCompletion(
            body_driven_config.generation_config,
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
                body_driven_config,
                generation_result.response_text,
                &result->turn_result,
                &result->repair_response_text,
                parse_error,
                sizeof(parse_error))) {
            result->turn_result = MakeFallbackTurnResult(generation_result.response_text, initial_session_state.language);
            result->used_turn_fallback = true;
        } else {
            result->used_turn_repair = true;
        }
    }

    if (initial_session_state.language != kGameLanguageEnglish &&
        LooksPredominantlyEnglish(result->turn_result.narration + " " + result->turn_result.clarification)) {
        if (!LocalizePlayerFacingText(
                body_driven_config,
                initial_session_state.language,
                0,
                0,
                &result->turn_result.narration,
                &result->turn_result.clarification,
                result)) {
            result->turn_result.narration = PlayerText(
                initial_session_state.language,
                "Your action was accepted, but the detailed report is temporarily unreadable.",
                "Votre action est prise en compte, mais le compte rendu détaillé demeure momentanément illisible.",
                "Handlingen din ble registrert, men den detaljerte rapporten er midlertidig uleselig.",
                "Din handling blev registreret, men den detaljerede rapport er midlertidigt ulæselig.",
                "Deine Aktion wurde berücksichtigt, aber der ausführliche Bericht ist vorübergehend unlesbar.",
                "La tua azione è stata registrata, ma il resoconto dettagliato è temporaneamente illeggibile.");
            result->turn_result.clarification = PlayerText(
                initial_session_state.language,
                "The validated world state is preserved despite the player-text localization failure.",
                "L'état validé du monde est conservé malgré l'échec de localisation du texte joueur.",
                "Den validerte verdenstilstanden bevares selv om lokaliseringen av spillerteksten mislyktes.",
                "Den validerede verdenstilstand bevares, selv om lokaliseringen af spillerteksten mislykkedes.",
                "Der bestätigte Weltzustand bleibt trotz der fehlgeschlagenen Lokalisierung des Spielertexts erhalten.",
                "Lo stato convalidato del mondo viene conservato nonostante l'errore di localizzazione del testo per il giocatore.");
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

        LlmGenerationConfig scene_config = body_driven_config.generation_config;
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
        CaptureSceneDebugArtifacts(
            result->updated_spatial_state,
            &result->turn_result.candidate_scene_text,
            &result->rendered_scene_text,
            &result->rendered_scene_debug_text);
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
    CaptureSceneDebugArtifacts(
        result->updated_spatial_state,
        0,
        &result->rendered_scene_text,
        &result->rendered_scene_debug_text);

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

void PrintHeadlessTurnDebugTrace(const HeadlessTurnResult& result, FILE* stream)
{
    FILE* out = stream ? stream : stdout;
    fprintf(out, "\n=== Turn Debug Trace ===\n");
    fprintf(out, "Initial place: %s\n", result.initial_place_id.empty() ? "(empty)" : result.initial_place_id.c_str());
    fprintf(out, "Updated place: %s\n", result.updated_place_id.empty() ? "(empty)" : result.updated_place_id.c_str());
    fprintf(out, "Intent: %s\n", result.turn_result.intent.empty() ? "(empty)" : result.turn_result.intent.c_str());
    fprintf(out, "Traversal requested: %s\n", result.traversal_requested ? "yes" : "no");
    if (result.traversal_requested) {
        fprintf(out, "Traversal direction: %s\n", CardinalDirectionToString(result.traversal_direction));
    }
    fprintf(out, "Generated room created: %s\n", result.generated_room_created ? "yes" : "no");
    fprintf(out, "Generated room cache refreshed: %s\n", result.generated_room_cache_refreshed ? "yes" : "no");
    fprintf(out, "Invisible barrier contact: %s\n", result.invisible_barrier_contact ? "yes" : "no");
    fprintf(out, "Turn fallback: %s\n", result.used_turn_fallback ? "yes" : "no");
    fprintf(out, "Turn repair: %s\n", result.used_turn_repair ? "yes" : "no");
    fprintf(out, "Thermal update used: %s\n", result.thermal_update_used ? "yes" : "no");
    fprintf(out, "Player-text localization used: %s\n", result.player_text_localization_used ? "yes" : "no");
    fprintf(out, "Effective LLM temperature: %.3f\n", result.effective_llm_temperature);
    fprintf(out, "Prompt tokens: %d\n", result.prompt_tokens);
    fprintf(out, "Generated tokens: %d\n", result.generated_tokens);
    fprintf(out, "Inference time: %.2f ms\n", result.inference_time_ms);

    fprintf(out, "\n--- Initial Spatial State ---\n");
    PrintSpatialStateSummary(result.initial_spatial_state, out);

    if (result.prospective_spatial_state_known) {
        fprintf(out, "\n--- Prospective Spatial State ---\n");
        PrintSpatialStateSummary(result.prospective_spatial_state, out);
    }

    PrintDebugBlock(out, "Engine Request", result.request_text);
    PrintDebugBlock(out, "Runtime Prompt", result.prompt_text);
    PrintDebugBlock(out, "Raw LLM Response", result.raw_response_text);
    if (!result.repair_response_text.empty()) {
        PrintDebugBlock(out, "Repair LLM Response", result.repair_response_text);
    }

    if (result.generated_room_created || result.generated_room_cache_refreshed) {
        const GeneratedRoom* generated_room = FindResultGeneratedRoom(result);
        if (generated_room) {
            fprintf(out, "\n--- Generated Room Spatial State ---\n");
            PrintSpatialStateSummary(generated_room->spatial_state, out);
            fprintf(out, "Generated room metadata source: %s\n", generated_room->metadata_fallback_used ? "fallback" : "llm");
            fprintf(out, "Generated room scene source: %s\n", generated_room->scene_source.empty() ? "(empty)" : generated_room->scene_source.c_str());
        }
    }

    PrintDebugBlock(out, "Scene Program", result.rendered_scene_text.empty() ? result.raw_scene_audit_response_text : result.rendered_scene_text);
    PrintDebugBlock(out, "Scene Compiler Report", result.rendered_scene_debug_text);

    fprintf(out, "\n--- Updated Spatial State ---\n");
    PrintSpatialStateSummary(result.updated_spatial_state, out);
    fprintf(out, "\n--- Updated Hard State ---\n");
    PrintHardStateSummary(result.updated_hard_state, out);
    fprintf(out, "\n--- Updated Soft State ---\n");
    PrintSoftStateSummary(result.updated_soft_state, out);
    fflush(out);
}

}  // namespace liminal
