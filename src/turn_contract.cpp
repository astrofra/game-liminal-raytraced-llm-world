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

static void AppendRecentHistory(std::string* text, const std::vector<SessionTurnRecord>* history)
{
    text->append("\nRecent session history\n");
    if (!history || history->empty()) {
        text->append("(none)\n");
        return;
    }

    const size_t max_entries = 4;
    const size_t start_index = history->size() > max_entries ? history->size() - max_entries : 0;
    for (size_t index = start_index; index < history->size(); ++index) {
        const SessionTurnRecord& record = (*history)[index];
        text->append("turn ");
        text->append(std::to_string(record.turn_number));
        text->append(" @ ");
        text->append(record.location_label.empty() ? LocationIdToString(record.location_id) : record.location_label.c_str());
        text->append("\n");
        AppendLabelValue(text, "command: ", record.player_command.c_str());
        AppendLabelValue(text, "intent: ", record.intent.c_str());
        AppendLabelValue(text, "narration: ", record.narration.c_str());
        if (!record.clarification.empty()) {
            AppendLabelValue(text, "clarification: ", record.clarification.c_str());
        }
    }
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
    text += "    \"move_count_changed\": boolean,\n";
    text += "    \"next_move_count\": integer,\n";
    text += "    \"score_changed\": boolean,\n";
    text += "    \"next_score\": integer,\n";
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

std::string BuildGeneratedRoomSchemaText()
{
    std::string text;
    text += "Return exactly one JSON object with these top-level keys:\n";
    text += "{\n";
    text += "  \"title\": string,\n";
    text += "  \"summary\": string,\n";
    text += "  \"arrival_narration\": string,\n";
    text += "  \"move_cost\": integer,\n";
    text += "  \"score_delta\": integer,\n";
    text += "  \"spatial_state\": {\n";
    text += "    \"location_archetype\": string,\n";
    text += "    \"time_of_day\": \"day\" | \"dusk\" | \"night\" | \"unknown\",\n";
    text += "    \"visibility_level\": \"clear\" | \"dusty\" | \"low\" | \"unknown\",\n";
    text += "    \"desert_state\": \"still\" | \"windy\" | \"dusty\" | \"unknown\",\n";
    text += "    \"interior_density\": \"sparse\" | \"dense\" | \"unknown\",\n";
    text += "    \"alert_level\": integer,\n";
    text += "    \"anchors\": [string],\n";
    text += "    \"visible_objects\": [string],\n";
    text += "    \"blocked_exits\": [\"north\" | \"east\" | \"south\" | \"west\"],\n";
    text += "    \"spatial_anomalies\": [string]\n";
    text += "  }\n";
    text += "}";
    return text;
}

std::string BuildSceneFormatRuleText()
{
    std::string text;
    text += "Scene v1 authoring rules for audit mode:\n";
    text += "- output ASCII only\n";
    text += "- one directive per line\n";
    text += "- do not use indented property blocks; every directive must be complete on one line\n";
    text += "- names must be quoted like \"roof\" or \"service_crate\"\n";
    text += "- allowed directives: room, camera, spotlight, sky, plane, box, prefab_gate, prefab_rack, prefab_crate, prefab_cooling_unit, prefab_ai_server\n";
    text += "- every scene must declare one room and one camera\n";
    text += "- keep geometry sparse and legible\n";
    text += "- prefer stable repeated objects through prefab_* directives\n";
    text += "- avoid long decorative lists of tiny objects\n";
    text += "- keep the place readable at 800x400 palette-limited noisy rendering\n";
    text += "- stay inside the datacenter + desert fiction; canonical lieux are examples, not limits\n";
    text += "- use gray(), not color() or opacity()\n";
    text += "- gray() controls luminance only; the engine applies locked semantic colors to sky, desert surfaces, and rack LEDs\n";
    text += "- prefer names like ground, desert_*, ridge_* or outcrop_* for exterior sand masses\n";
    text += "- valid examples:\n";
    text += "  room \"datacenter roof watch\"\n";
    text += "  camera eye(0.0,1.75,-7.9) target(0.0,1.18,8.8) up(0.0,1.0,0.0) fov(46.0)\n";
    text += "  spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(34.0) cone(12.0,28.0) intensity(70.0)\n";
    text += "  sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(77)\n";
    text += "  plane \"roof\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(16.0,24.0) gray(0.14)\n";
    text += "  box \"parapet_front\" pos(0.0,0.55,1.6) size(14.0,1.1,0.7) gray(0.26)\n";
    text += "  prefab_crate \"service_crate\" pos(2.4,0.55,-1.8) size(1.7,1.1,1.4) gray(0.20) detail(0.31)\n";
    text += "  prefab_cooling_unit \"vent_stack_left\" pos(-4.3,1.0,-3.2) size(1.0,2.0,1.0) gray(0.30) detail(0.39)\n";
    text += "  prefab_ai_server \"inference_mainframe\" pos(4.1,1.40,-2.2) size(1.7,2.8,1.7) gray(0.16) detail(0.28)\n";
    return text;
}

std::string BuildSpatialBriefText(const SpatialState& spatial_state)
{
    std::string text;
    AppendLabelValue(&text, "location_id: ", LocationIdToString(spatial_state.location_id));
    AppendLabelValue(
        &text,
        "room_title: ",
        spatial_state.room_title.empty() ? "unknown" : spatial_state.room_title.c_str());
    AppendLabelValue(
        &text,
        "room_summary: ",
        spatial_state.room_summary.empty() ? "unknown" : spatial_state.room_summary.c_str());
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
    const std::vector<SessionTurnRecord>* recent_history,
    const char* player_command,
    bool include_candidate_scene_text)
{
    std::string text;
    text += "You are the structured turn engine for a local interactive fiction prototype.\n";
    text += "Return concise playable output. Preserve hard-state continuity. Do not write markdown.\n";
    text += "Do not invent new locations outside gate, server_aisles, roof_watch unless explicitly required.\n";
    if (spatial_state.location_id == kLocationUnknown) {
        text += "The current room may be an improvised generated room. Keep that room stable unless the player explicitly moves.\n";
    }
    text += "Narration must stay short, concrete, and spatially actionable.\n";
    text += "Prefer 2 to 4 short sentences and stay roughly under 70 words.\n";
    text += "Avoid long atmospheric digressions and avoid restating the whole room summary.\n";
    text += "move_count tracks effectful actions only. Increase it only when the command materially changes position, inventory, state or knowledge.\n";
    text += "score should stay conservative and stable. Increase it only for meaningful progress, discoveries, access breakthroughs or solved obstacles.\n";
    text += "Whenever the room contains usable objects or interfaces, mention at least one concrete actionable affordance.\n";
    text += "Prefer nouns the player can inspect, open, read, unlock, press, pull, enter, take, or switch.\n";
    text += "Good actionable examples: hatch, crate, keypad, badge reader, placard, cabinet, switch, console, intercom, lockbox.\n";
    text += "If visible_objects are present in the spatial brief, reuse at least one of them directly in the narration when relevant.\n";
    text += "When the room topology is clear, keep spatial_delta.blocked_exits accurate so closed cardinal directions stay closed.\n";
    text += "If topology is clear, explicitly decide which cardinal directions are blocked instead of leaving the room ambiguous.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "move_count: ", std::to_string(hard_state.move_count).c_str());
    AppendLabelValue(&text, "score: ", std::to_string(hard_state.score).c_str());
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
    AppendRecentHistory(&text, recent_history);

    text += "\nCurrent spatial brief\n";
    text += BuildSpatialBriefText(spatial_state);

    text += "\nPlayer command\n";
    AppendLabelValue(&text, "command: ", player_command ? player_command : "look");

    text += "\nStructured output schema\n";
    text += BuildTurnResultSchemaText();

    if (include_candidate_scene_text) {
        text += "\n\nRequired debug field\n";
        text += "For this turn, include one extra top-level key named \"candidate_scene_text\" containing a full .scene candidate as a string.\n";
        text += "Keep it consistent with the spatial delta.\n";
        text += "Do not omit this field.\n";
        text += "Encode the full multi-line .scene program inside one JSON string using escaped newlines.\n";
        text += BuildSceneFormatRuleText();
    }

    return text;
}

std::string BuildGeneratedRoomPrompt(
    const HardState& hard_state,
    const SoftState& soft_state,
    const SpatialState& current_spatial_state,
    const std::vector<SessionTurnRecord>* recent_history,
    CardinalDirection direction)
{
    std::string text;
    text += "You invent one neighboring room for a local interactive-fiction prototype.\n";
    text += "Return valid JSON only. Do not write markdown fences. Do not return explanations.\n";
    text += "The new room must stay inside the same liminal datacenter / desert fiction.\n";
    text += "It must be spatially readable, sparse, and suitable for grayscale raytracing.\n";
    text += "Do not describe a whole region. Describe one immediate neighboring room only.\n";
    text += "The reverse direction back to the source room should usually remain possible.\n";
    text += "Keep the title short.\n";
    text += "Keep the summary compact and concrete.\n";
    text += "Keep arrival_narration under about 60 words and make it immediately playable.\n";
    text += "Set move_cost to 1 for a normal successful traversal, and use 0 only if the traversal has no meaningful effect.\n";
    text += "Set score_delta conservatively. Use 0 unless entering this room meaningfully advances progress or reveals something important.\n";
    text += "visible_objects must contain 3 to 5 concrete actionable objects or interfaces, not vague scenery only.\n";
    text += "Include at least two directly usable things such as a hatch, crate, keypad, badge reader, placard, switch, cabinet, console, lockbox or intercom.\n";
    text += "Use blocked_exits to mark closed directions explicitly; directions not listed there will be treated as traversable.\n";
    text += "For every new room, decide all four cardinal directions. blocked_exits must list every blocked direction explicitly.\n";
    text += "The reverse direction back to the source room should usually stay open, so it should usually be absent from blocked_exits.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "move_count: ", std::to_string(hard_state.move_count).c_str());
    AppendLabelValue(&text, "score: ", std::to_string(hard_state.score).c_str());
    AppendLabelValue(&text, "alert_level: ", std::to_string(hard_state.alert_level).c_str());
    AppendLabelValue(&text, "cooling_state: ", ResourceStateToString(hard_state.cooling_state));
    AppendLabelValue(&text, "water_state: ", ResourceStateToString(hard_state.water_state));
    AppendLabelValue(&text, "power_state: ", ResourceStateToString(hard_state.power_state));
    AppendStringList(&text, "unresolved_threats: ", hard_state.unresolved_threats);

    text += "\nCurrent soft state\n";
    AppendLabelValue(&text, "rolling_summary: ", soft_state.rolling_summary.c_str());
    AppendLabelValue(&text, "atmosphere: ", soft_state.atmosphere.c_str());
    AppendRecentHistory(&text, recent_history);

    text += "\nCurrent room brief\n";
    text += BuildSpatialBriefText(current_spatial_state);

    text += "\nTraversal request\n";
    AppendLabelValue(&text, "direction: ", CardinalDirectionToString(direction));

    text += "\nGenerated room schema\n";
    text += BuildGeneratedRoomSchemaText();
    return text;
}

std::string BuildGeneratedRoomScenePrompt(
    const SpatialState& current_spatial_state,
    const SpatialState& generated_spatial_state,
    CardinalDirection direction)
{
    std::string text;
    text += "You are in scene-audit mode for a newly discovered neighboring room.\n";
    text += "Return only a .scene program, no markdown, no explanations.\n";
    text += BuildSceneFormatRuleText();
    text += "\nSource room brief\n";
    text += BuildSpatialBriefText(current_spatial_state);
    text += "\nTraversal direction\n";
    AppendLabelValue(&text, "direction: ", CardinalDirectionToString(direction));
    text += "\nNew room brief\n";
    text += BuildSpatialBriefText(generated_spatial_state);
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
