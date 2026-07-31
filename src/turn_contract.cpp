#include "turn_contract.h"

#include <stdio.h>

namespace liminal {

namespace {

static void AppendLabelValue(std::string* text, const char* label, const char* value)
{
    text->append(label);
    text->append(value ? value : "");
    text->append("\n");
}

static void AppendStringList(std::string* text, const char* label, const std::vector<std::string>& values)
{
    text->append(label);
    if (values.empty()) {
        text->append("(none)\n");
        return;
    }

    for (size_t index = 0; index < values.size(); ++index) {
        text->append(index == 0 ? "" : ", ");
        text->append(values[index]);
    }
    text->append("\n");
}

}  // namespace

std::string BuildTurnResultSchemaText()
{
    std::string text;
    text += "Return exactly one JSON object with these top-level keys:\n";
    text += "{\n";
    text += "  \"intent\": string,\n";
    text += "  \"narration\": string,\n";
    text += "  \"clarification\": string,\n";
    text += "  \"hard_state_delta\": {\n";
    text += "    \"location_changed\": boolean,\n";
    text += "    \"next_location_id\": \"gate\" | \"server_aisles\" | \"roof_watch\" | \"unknown\",\n";
    text += "    \"alert_level_changed\": boolean,\n";
    text += "    \"next_alert_level\": integer,\n";
    text += "    \"cooling_state_changed\": boolean,\n";
    text += "    \"next_cooling_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"water_state_changed\": boolean,\n";
    text += "    \"next_water_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"power_state_changed\": boolean,\n";
    text += "    \"next_power_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"inventory_add\": [string],\n";
    text += "    \"inventory_remove\": [string],\n";
    text += "    \"threats_add\": [string],\n";
    text += "    \"threats_remove\": [string]\n";
    text += "  },\n";
    text += "  \"spatial_delta\": {\n";
    text += "    \"location_changed\": boolean,\n";
    text += "    \"next_location_id\": \"gate\" | \"server_aisles\" | \"roof_watch\" | \"unknown\",\n";
    text += "    \"time_of_day_changed\": boolean,\n";
    text += "    \"next_time_of_day\": \"day\" | \"dusk\" | \"night\" | \"unknown\",\n";
    text += "    \"visibility_changed\": boolean,\n";
    text += "    \"next_visibility_level\": \"clear\" | \"dusty\" | \"low\" | \"unknown\",\n";
    text += "    \"desert_state_changed\": boolean,\n";
    text += "    \"next_desert_state\": \"still\" | \"windy\" | \"dusty\" | \"unknown\",\n";
    text += "    \"interior_density_changed\": boolean,\n";
    text += "    \"next_interior_density\": \"sparse\" | \"dense\" | \"unknown\",\n";
    text += "    \"alert_level_changed\": boolean,\n";
    text += "    \"next_alert_level\": integer,\n";
    text += "    \"anchors_present\": [string],\n";
    text += "    \"visible_objects\": [string],\n";
    text += "    \"blocked_exits\": [string],\n";
    text += "    \"spatial_anomalies\": [string],\n";
    text += "    \"scene_constraints\": [string]\n";
    text += "  },\n";
    text += "  \"continuity_notes\": [string]";
    text += "\n}";
    return text;
}

std::string BuildSceneFormatRuleText()
{
    std::string text;
    text += "Scene v1 authoring rules for audit mode:\n";
    text += "- output ASCII only\n";
    text += "- one directive per line\n";
    text += "- allowed directives: room, camera, spotlight, sky, plane, box, prefab_gate, prefab_rack, prefab_crate, prefab_cooling_unit\n";
    text += "- every scene must declare one room and one camera\n";
    text += "- keep geometry sparse and legible\n";
    text += "- prefer stable repeated objects through prefab_* directives\n";
    text += "- avoid long decorative lists of tiny objects\n";
    text += "- keep the place readable at 800x400 grayscale noisy rendering\n";
    text += "- stay inside the datacenter fiction and the three canonical lieux\n";
    return text;
}

std::string BuildSpatialBriefText(const SpatialState& spatial_state)
{
    std::string text;
    AppendLabelValue(&text, "location_id: ", LocationIdToString(spatial_state.location_id));
    AppendLabelValue(
        &text,
        "location_archetype: ",
        spatial_state.location_archetype.empty() ? "unknown" : spatial_state.location_archetype.c_str());
    AppendLabelValue(
        &text,
        "canonical_fixture: ",
        spatial_state.canonical_fixture.empty() ? "unknown" : spatial_state.canonical_fixture.c_str());
    AppendLabelValue(&text, "time_of_day: ", TimeOfDayToString(spatial_state.time_of_day));
    AppendLabelValue(&text, "visibility_level: ", VisibilityLevelToString(spatial_state.visibility_level));
    AppendLabelValue(&text, "desert_state: ", DesertStateToString(spatial_state.desert_state));
    AppendLabelValue(&text, "interior_density: ", InteriorDensityToString(spatial_state.interior_density));

    char alert_buffer[64];
    snprintf(alert_buffer, sizeof(alert_buffer), "%d", spatial_state.alert_level);
    AppendLabelValue(&text, "alert_level: ", alert_buffer);

    AppendStringList(&text, "anchors: ", spatial_state.anchors);
    AppendStringList(&text, "visible_objects: ", spatial_state.visible_objects);
    AppendStringList(&text, "blocked_exits: ", spatial_state.blocked_exits);
    AppendStringList(&text, "spatial_anomalies: ", spatial_state.spatial_anomalies);
    return text;
}

std::string BuildTurnPrompt(
    const HardState& hard_state,
    const SoftState& soft_state,
    const SpatialState& spatial_state,
    const char* player_command,
    bool include_candidate_scene_text)
{
    std::string text;
    text += "You are the structured turn engine for a local interactive fiction prototype.\n";
    text += "Return concise playable output. Preserve hard-state continuity. Do not write markdown.\n";
    text += "Do not invent new locations outside gate, server_aisles, roof_watch unless explicitly required.\n";
    text += "Narration must stay short, concrete, and spatially actionable.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "current_location_id: ", LocationIdToString(hard_state.current_location_id));
    AppendLabelValue(&text, "alert_level: ", std::to_string(hard_state.alert_level).c_str());
    AppendLabelValue(&text, "cooling_state: ", ResourceStateToString(hard_state.cooling_state));
    AppendLabelValue(&text, "water_state: ", ResourceStateToString(hard_state.water_state));
    AppendLabelValue(&text, "power_state: ", ResourceStateToString(hard_state.power_state));
    AppendStringList(&text, "inventory_items: ", hard_state.inventory_items);
    AppendStringList(&text, "named_entities: ", hard_state.named_entities);
    AppendStringList(&text, "unresolved_threats: ", hard_state.unresolved_threats);

    text += "\nCurrent soft state\n";
    AppendLabelValue(&text, "rolling_summary: ", soft_state.rolling_summary.c_str());
    AppendLabelValue(&text, "atmosphere: ", soft_state.atmosphere.c_str());
    AppendStringList(&text, "active_hypotheses: ", soft_state.active_hypotheses);
    AppendStringList(&text, "tolerated_incoherences: ", soft_state.tolerated_incoherences);

    text += "\nCurrent spatial brief\n";
    text += BuildSpatialBriefText(spatial_state);

    text += "\nPlayer command\n";
    AppendLabelValue(&text, "command: ", player_command ? player_command : "look");

    text += "\nStructured output schema\n";
    text += BuildTurnResultSchemaText();

    if (include_candidate_scene_text) {
        text += "\n\nOptional debug field\n";
        text += "You may append one extra key named \"candidate_scene_text\" containing a full .scene candidate as a string.\n";
        text += "Only include it if you can keep it consistent with the spatial delta.\n";
        text += BuildSceneFormatRuleText();
    }

    return text;
}

std::string BuildSceneAuditPrompt(const SpatialState& spatial_state)
{
    std::string text;
    text += "You are in scene-audit mode.\n";
    text += "Return only a .scene program, no markdown, no explanations.\n";
    text += BuildSceneFormatRuleText();
    text += "\nSpatial brief\n";
    text += BuildSpatialBriefText(spatial_state);
    return text;
}

}  // namespace liminal
