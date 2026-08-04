#include "turn_contract.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

static void AppendWorldMotif(std::vector<std::string>* values, const char* value)
{
    if (!values || !value || !value[0]) {
        return;
    }

    for (size_t index = 0; index < values->size(); ++index) {
        if ((*values)[index] == value) {
            return;
        }
    }
    values->push_back(value);
}

static uint32_t HashSpatialVariationSeed(const SpatialState& spatial_state)
{
    uint32_t hash = 2166136261u;
    if (spatial_state.world_pose_known) {
        const int world_x = static_cast<int>(floorf(spatial_state.world_x * 10.0f + (spatial_state.world_x >= 0.0f ? 0.5f : -0.5f)));
        const int world_z = static_cast<int>(floorf(spatial_state.world_z * 10.0f + (spatial_state.world_z >= 0.0f ? 0.5f : -0.5f)));
        hash ^= static_cast<uint32_t>(world_x);
        hash *= 16777619u;
        hash ^= static_cast<uint32_t>(world_z);
        hash *= 16777619u;
    }

    const std::string basis = spatial_state.location_archetype + "|" + spatial_state.room_title + "|" + spatial_state.room_summary;
    for (size_t index = 0; index < basis.size(); ++index) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(basis[index]));
        hash *= 16777619u;
    }
    return hash ? hash : 1u;
}

static float NormalizeDegrees(float degrees)
{
    while (degrees < 0.0f) {
        degrees += 360.0f;
    }
    while (degrees >= 360.0f) {
        degrees -= 360.0f;
    }
    return degrees;
}

static int ComputeSpatialOctant(const SpatialState& spatial_state)
{
    if (!spatial_state.world_pose_known) {
        return 0;
    }

    const float normalized = NormalizeDegrees(ComputeSpatialWorldAngleDegrees(spatial_state));
    const int octant = static_cast<int>(floorf((normalized + 22.5f) / 45.0f)) % 8;
    return octant < 0 ? octant + 8 : octant;
}

static const char* DescribeSpatialBearingNarrative(const SpatialState& spatial_state)
{
    if (!spatial_state.world_pose_known) {
        return "Its bearing around the datacenter is unknown, so distinguish it with one clear local motif instead of generic filler.";
    }

    switch (ComputeSpatialOctant(spatial_state)) {
    case 0:
        return "It sits on the direct gate approach, so frontal threshold silhouettes, inspection cues and road-facing hardware make sense.";
    case 1:
        return "It sits between the gate approach and the east side, favoring oblique service traces, offset fixtures and a slight lateral lean.";
    case 2:
        return "It sits on the east service flank, so side-running infrastructure and asymmetry pulling east are a good fit.";
    case 3:
        return "It sits between the east flank and the datacenter rear, where crosswind exposure and rearward service masses should start to dominate.";
    case 4:
        return "It sits on the far side opposite the entry gate, where rear bastions, exhaust masses and long horizon cuts are plausible.";
    case 5:
        return "It sits between the rear line and the west flank, favoring shadowed recesses, oblique cover and weather-worn service remains.";
    case 6:
        return "It sits on the west weather flank, so wind-shadow details, rough cover and laterally offset maintenance cues fit best.";
    case 7:
        return "It sits between the west flank and the gate approach, where caution hardware and drifting exterior traces can mix with threshold leftovers.";
    default:
        return "Its bearing is ambiguous; prefer a strong side bias instead of a mirrored room.";
    }
}

static const char* DescribeSpatialBandNarrative(const SpatialState& spatial_state)
{
    const char* world_band = DescribeSpatialWorldBand(spatial_state);
    if (strcmp(world_band, "central core") == 0) {
        return "Treat it as buried inside the datacenter mass: enclosed, technical, compressed, and dominated by hard infrastructure.";
    }
    if (strcmp(world_band, "inner technical ring") == 0) {
        return "Treat it as service-heavy datacenter territory: enclosed but uneven, with technical clutter, airflow, and machine-scale hints.";
    }
    if (strcmp(world_band, "perimeter seam") == 0) {
        return "Treat it as a mixed threshold where the datacenter shell starts leaking exterior cues through vents, slits, decks or broken edges.";
    }
    if (strcmp(world_band, "outer parapet") == 0) {
        return "Treat it as exposed perimeter architecture: open sky, warning hardware, roof access, and one strong datacenter mass against the horizon.";
    }
    if (strcmp(world_band, "open desert") == 0) {
        return "Treat it as open desert margin: sparse cover, buried infrastructure remnants, navigation cues, and no generic sealed ceiling.";
    }
    return "Treat it as a liminal neighboring space and distinguish it through a single dominant architectural or desert-facing mass.";
}

static void BuildSpatialVariationMotifs(
    const SpatialState& spatial_state,
    std::vector<std::string>* favor_terms,
    std::vector<std::string>* avoid_terms)
{
    if (!favor_terms || !avoid_terms) {
        return;
    }

    const char* world_band = DescribeSpatialWorldBand(spatial_state);
    if (strcmp(world_band, "central core") == 0) {
        AppendWorldMotif(favor_terms, "service panel");
        AppendWorldMotif(favor_terms, "maintenance hatch");
        AppendWorldMotif(favor_terms, "relay cabinet");
        AppendWorldMotif(favor_terms, "warning placard");
        AppendWorldMotif(favor_terms, "cooling vent");
        AppendWorldMotif(avoid_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "cactus cluster");
        AppendWorldMotif(avoid_terms, "rock outcrop");
    } else if (strcmp(world_band, "inner technical ring") == 0) {
        AppendWorldMotif(favor_terms, "service crate");
        AppendWorldMotif(favor_terms, "cooling vent");
        AppendWorldMotif(favor_terms, "relay cabinet");
        AppendWorldMotif(favor_terms, "maintenance hatch");
        AppendWorldMotif(favor_terms, "ai server column");
        AppendWorldMotif(avoid_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "cactus cluster");
    } else if (strcmp(world_band, "perimeter seam") == 0) {
        AppendWorldMotif(favor_terms, "roof hatch");
        AppendWorldMotif(favor_terms, "warning beacon");
        AppendWorldMotif(favor_terms, "service crate");
        AppendWorldMotif(favor_terms, "maintenance hatch");
        AppendWorldMotif(favor_terms, "warning placard");
        AppendWorldMotif(avoid_terms, "dense rack maze");
        AppendWorldMotif(avoid_terms, "sealed vault");
    } else if (strcmp(world_band, "outer parapet") == 0) {
        AppendWorldMotif(favor_terms, "roof hatch");
        AppendWorldMotif(favor_terms, "warning beacon");
        AppendWorldMotif(favor_terms, "service crate");
        AppendWorldMotif(favor_terms, "maintenance hatch");
        AppendWorldMotif(favor_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "sealed ceiling");
        AppendWorldMotif(avoid_terms, "dense rack maze");
    } else if (strcmp(world_band, "open desert") == 0) {
        AppendWorldMotif(favor_terms, "range marker");
        AppendWorldMotif(favor_terms, "survey cache");
        AppendWorldMotif(favor_terms, "buried service hatch");
        AppendWorldMotif(favor_terms, "rock outcrop");
        AppendWorldMotif(favor_terms, "cactus cluster");
        AppendWorldMotif(avoid_terms, "badge reader");
        AppendWorldMotif(avoid_terms, "dense rack maze");
        AppendWorldMotif(avoid_terms, "sealed ceiling");
    }

    const char* gate_relation = DescribeSpatialGateRelation(spatial_state);
    if (strcmp(gate_relation, "entry-facing side") == 0) {
        AppendWorldMotif(favor_terms, "checkpoint gate");
        AppendWorldMotif(favor_terms, "warning placard");
        AppendWorldMotif(favor_terms, "service crate");
    } else if (strcmp(gate_relation, "east flank") == 0) {
        AppendWorldMotif(favor_terms, "east service cabinet");
        AppendWorldMotif(favor_terms, "east warning beacon");
        AppendWorldMotif(favor_terms, "east maintenance hatch");
    } else if (strcmp(gate_relation, "west flank") == 0) {
        AppendWorldMotif(favor_terms, "west service crate");
        AppendWorldMotif(favor_terms, "west warning placard");
        AppendWorldMotif(favor_terms, "west rock outcrop");
    } else if (strcmp(gate_relation, "far side opposite the entry gate") == 0) {
        AppendWorldMotif(favor_terms, "rear hatch");
        AppendWorldMotif(favor_terms, "rear warning beacon");
        AppendWorldMotif(favor_terms, "rear service cabinet");
    }

    static const char* const kOctantVariants[8][3] = {
        { "front inspection placard", "approach-side service crate", "front maintenance hatch" },
        { "angled warning beacon", "east-leaning service crate", "lateral placard" },
        { "east relay cabinet", "east maintenance hatch", "east warning marker" },
        { "rear-east beacon", "crosswind hatch", "offset service cabinet" },
        { "rear service crate", "rear hatch", "backline warning beacon" },
        { "rear-west outcrop", "shadowed service crate", "west rear marker" },
        { "west warning placard", "west maintenance hatch", "wind-shadow rock outcrop" },
        { "gate-west barrier", "angled service crate", "west-front beacon" },
    };
    const int octant = ComputeSpatialOctant(spatial_state);
    const uint32_t seed = HashSpatialVariationSeed(spatial_state);
    AppendWorldMotif(favor_terms, kOctantVariants[octant][seed % 3u]);
}

static std::string BuildSpatialVariationGuideText(const SpatialState& spatial_state)
{
    std::string text;
    text += "Hidden spatial drift guide\n";
    AppendLabelValue(&text, "world_band: ", DescribeSpatialWorldBand(spatial_state));
    AppendLabelValue(&text, "gate_relation: ", DescribeSpatialGateRelation(spatial_state));
    AppendLabelValue(&text, "sky_exposure: ", DescribeSpatialSkyExposure(spatial_state));

    AppendLabelValue(&text, "radial_narrative: ", DescribeSpatialBandNarrative(spatial_state));
    AppendLabelValue(&text, "bearing_narrative: ", DescribeSpatialBearingNarrative(spatial_state));

    std::vector<std::string> favor_terms;
    std::vector<std::string> avoid_terms;
    BuildSpatialVariationMotifs(spatial_state, &favor_terms, &avoid_terms);
    AppendStringList(&text, "favor_terms: ", favor_terms);
    AppendStringList(&text, "avoid_terms: ", avoid_terms);
    text += "metadata_pressure: let title, location_archetype, anchors, visible_objects and scene_constraints echo at least two favor_terms.\n";
    text += "layout_pressure: prefer one dominant asymmetry and side-qualified constraints such as east hatch, rear beacon, west crate or central console.\n";
    text += "anti_repetition: do not collapse unrelated rooms into the same generic room family if the drift guide points elsewhere.\n";
    return text;
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
    text += "    \"temperature_changed\": boolean,\n";
    text += "    \"next_datacenter_temperature_c\": integer,\n";
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
    text += "  \"next_datacenter_temperature_c\": integer,\n";
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
    text += "    \"spatial_anomalies\": [string],\n";
    text += "    \"scene_constraints\": [string]\n";
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
    text += "- allowed directives: room, camera, spotlight, sky, plane, box, prefab_gate, prefab_rack, prefab_crate, prefab_cooling_unit, prefab_ai_server, prefab_cactus_sentinel, prefab_cactus_fork, prefab_cactus_cluster, prefab_rock_low, prefab_rock_wide, prefab_rock_tall, prefab_rock_spire\n";
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
    text += "  prefab_cactus_fork \"cactus_watch\" pos(-5.4,1.2,7.8) size(1.2,2.4,1.2) gray(0.25)\n";
    text += "  prefab_rock_wide \"rock_shelf\" pos(4.6,0.7,6.8) size(2.0,1.4,1.6) gray(0.23)\n";
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
    AppendLabelValue(&text, "world_band: ", DescribeSpatialWorldBand(spatial_state));
    AppendLabelValue(&text, "gate_relation: ", DescribeSpatialGateRelation(spatial_state));
    AppendLabelValue(&text, "sky_exposure: ", DescribeSpatialSkyExposure(spatial_state));

    char alert_buffer[64];
    snprintf(alert_buffer, sizeof(alert_buffer), "%d", spatial_state.alert_level);
    AppendLabelValue(&text, "alert_level: ", alert_buffer);

    AppendStringList(&text, "anchors: ", spatial_state.anchors);
    AppendStringList(&text, "visible_objects: ", spatial_state.visible_objects);
    AppendStringList(&text, "blocked_exits: ", spatial_state.blocked_exits);
    AppendStringList(&text, "spatial_anomalies: ", spatial_state.spatial_anomalies);
    AppendStringList(&text, "scene_constraints: ", spatial_state.scene_constraints);
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
    text += "When the decor composition becomes clearer or changes materially, update spatial_delta.scene_constraints with short layout cues, never coordinates.\n";
    text += "Good scene_constraints examples: hero ai server, rack bank, cooling flank, central console, rear hatch, service crate, checkpoint gate, open horizon, keep corridor clear.\n";
    text += "datacenter_temperature_c is the ambient technical temperature in Celsius. Let it influence the feel of the prose, the airflow, the discomfort, or the operational tension.\n";
    text += "Use temperature as a narrative cue, not as a reason to invent new geometry or random props.\n";
    text += "Set hard_state_delta.temperature_changed only when heat, cooling, airflow, load, or power conditions materially shift.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "move_count: ", std::to_string(hard_state.move_count).c_str());
    AppendLabelValue(&text, "score: ", std::to_string(hard_state.score).c_str());
    AppendLabelValue(&text, "current_location_id: ", LocationIdToString(hard_state.current_location_id));
    AppendLabelValue(&text, "alert_level: ", std::to_string(hard_state.alert_level).c_str());
    AppendLabelValue(
        &text,
        "datacenter_temperature_c: ",
        std::to_string(hard_state.datacenter_temperature_c).c_str());
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
    const SpatialState& prospective_spatial_state,
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
    text += "When the room suggests compute, model storage, restricted inference, accelerator hardware or machine authority, include a concrete machine-scale cue such as inference mainframe, ai server column, accelerator cabinet or tensor stack.\n";
    text += "If such a machine dominates the room, prefer scene_constraints containing hero ai server.\n";
    text += "scene_constraints should contain 1 to 4 short decor cues for the engine, describing dominant masses or layout bias, never coordinates.\n";
    text += "Good scene_constraints examples: hero ai server, rack bank, cooling flank, central console, rear hatch, service crate, checkpoint gate, open horizon, keep corridor clear.\n";
    text += "datacenter_temperature_c should shape title, summary, and arrival_narration through heat, air, hum, or strain, without turning it directly into scene geometry.\n";
    text += "Set next_datacenter_temperature_c to the temperature that should remain on the HUD after entering the room. Keep it close to the current value unless the room is materially hotter or colder.\n";
    text += "The engine maintains a hidden spatial drift map around the datacenter. Treat the qualitative world cues below as ground truth.\n";
    text += "If the target world band suggests outer parapet or open desert, avoid inventing a sealed ceiling.\n";
    text += "If the target world band suggests central core or inner technical ring, prefer enclosed datacenter rooms unless there is a strong reason not to.\n";
    text += "At the perimeter seam, partial openings, vents, service decks, broken roofs, or side exposures are plausible.\n";
    text += "When the target world band is open desert, use desert-facing objects and cues, not only interior datacenter interfaces.\n";
    text += "Do not reuse a generic location_archetype across unrelated rooms. Let the hidden drift cues change the lexical family of the room.\n";
    text += "If the drift guide suggests east, west, front or rear asymmetry, reflect it verbally in anchors or scene_constraints.\n";
    text += "Scene constraints may include side words such as east, west, rear, front or central; keep them qualitative, not numeric.\n";
    text += "Use blocked_exits to mark closed directions explicitly; directions not listed there will be treated as traversable.\n";
    text += "For every new room, decide all four cardinal directions. blocked_exits must list every blocked direction explicitly.\n";
    text += "The reverse direction back to the source room should usually stay open, so it should usually be absent from blocked_exits.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "move_count: ", std::to_string(hard_state.move_count).c_str());
    AppendLabelValue(&text, "score: ", std::to_string(hard_state.score).c_str());
    AppendLabelValue(&text, "alert_level: ", std::to_string(hard_state.alert_level).c_str());
    AppendLabelValue(
        &text,
        "datacenter_temperature_c: ",
        std::to_string(hard_state.datacenter_temperature_c).c_str());
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
    text += "\nTarget room world cues\n";
    text += BuildSpatialBriefText(prospective_spatial_state);
    text += "\nTarget room drift guide\n";
    text += BuildSpatialVariationGuideText(prospective_spatial_state);

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
    text += "\nNew room drift guide\n";
    text += BuildSpatialVariationGuideText(generated_spatial_state);
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
    text += "\nHidden spatial drift guide\n";
    text += BuildSpatialVariationGuideText(spatial_state);
    return text;
}

}  // namespace liminal
