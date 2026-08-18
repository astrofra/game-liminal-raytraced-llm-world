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
        return "Its bearing in the Erycinian survey field is unknown, so distinguish it with one clear datum, quarry mass, or instrument motif.";
    }

    switch (ComputeSpatialOctant(spatial_state)) {
    case 0:
        return "It sits on the baseward approach, so frontal threshold silhouettes, split gate masses, and route datums make sense.";
    case 1:
        return "It sits between the base approach and east quarry flank, favoring oblique retaining slabs and offset survey hardware.";
    case 2:
        return "It sits on the east quarry flank, so lateral extraction traces and an asymmetry pulling east are a good fit.";
    case 3:
        return "It sits between the east flank and deep field, where open sky, sparse datums, and low horizon masses should dominate.";
    case 4:
        return "It sits deep beyond the survey base, where long quarry cuts, isolated beacons, and ambiguous open ground are plausible.";
    case 5:
        return "It sits between deep field and the west flank, favoring wind-scoured ledges, oblique cover, and abandoned prospecting traces.";
    case 6:
        return "It sits on the west weather flank, so wind-shadow details, rough quarry cover, and laterally offset route marks fit best.";
    case 7:
        return "It sits between the west flank and the base approach, where warning hardware, cargo, and threshold remnants can mix.";
    default:
        return "Its bearing is ambiguous; prefer a strong side bias instead of a mirrored room.";
    }
}

static const char* DescribeSpatialBandNarrative(const SpatialState& spatial_state)
{
    const char* world_band = DescribeSpatialWorldBand(spatial_state);
    if (strcmp(world_band, "survey camp") == 0) {
        return "Treat it as the human survey base: compressed, pressure-serviced, and dominated by a few heavy brutalist masses.";
    }
    if (strcmp(world_band, "inner quarry") == 0) {
        return "Treat it as worked quarry ground: retaining slabs, extraction equipment, sample handling, and legible return routes.";
    }
    if (strcmp(world_band, "quarry seam") == 0) {
        return "Treat it as a seam between human works and the field: exposed crystal, scanning instruments, broken cuts, and partial shelter.";
    }
    if (strcmp(world_band, "outer shelf") == 0) {
        return "Treat it as an exposed quarry shelf: open sky, sparse route beacons, and one severe human mass against the horizon.";
    }
    if (strcmp(world_band, "open venus") == 0) {
        return "Treat it as open Venusian field: hostile emptiness, sparse datums, low geology, and no generic sealed enclosure.";
    }
    return "Treat it as a liminal Erycinian place and distinguish it through one dominant quarry mass, datum, or instrument.";
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
    if (strcmp(world_band, "survey camp") == 0) {
        AppendWorldMotif(favor_terms, "prospecting shelter");
        AppendWorldMotif(favor_terms, "atmospheric processor");
        AppendWorldMotif(favor_terms, "sample case");
        AppendWorldMotif(favor_terms, "route slate");
        AppendWorldMotif(favor_terms, "heavy cantilever");
        AppendWorldMotif(avoid_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "server rack");
        AppendWorldMotif(avoid_terms, "office clutter");
    } else if (strcmp(world_band, "inner quarry") == 0) {
        AppendWorldMotif(favor_terms, "extraction rig");
        AppendWorldMotif(favor_terms, "retaining fin");
        AppendWorldMotif(favor_terms, "sample crate");
        AppendWorldMotif(favor_terms, "quarry pylon");
        AppendWorldMotif(favor_terms, "cut floor");
        AppendWorldMotif(avoid_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "server rack");
    } else if (strcmp(world_band, "quarry seam") == 0) {
        AppendWorldMotif(favor_terms, "crystal cluster");
        AppendWorldMotif(favor_terms, "crystal scanner");
        AppendWorldMotif(favor_terms, "survey beacon");
        AppendWorldMotif(favor_terms, "broken quarry cut");
        AppendWorldMotif(favor_terms, "instrument deck");
        AppendWorldMotif(avoid_terms, "visible alien wall");
        AppendWorldMotif(avoid_terms, "dense machinery maze");
    } else if (strcmp(world_band, "outer shelf") == 0) {
        AppendWorldMotif(favor_terms, "survey beacon");
        AppendWorldMotif(favor_terms, "quarry pylon");
        AppendWorldMotif(favor_terms, "low horizon mass");
        AppendWorldMotif(favor_terms, "sample case");
        AppendWorldMotif(favor_terms, "open horizon");
        AppendWorldMotif(avoid_terms, "sealed ceiling");
        AppendWorldMotif(avoid_terms, "visible alien wall");
    } else if (strcmp(world_band, "open venus") == 0) {
        AppendWorldMotif(favor_terms, "distant survey light");
        AppendWorldMotif(favor_terms, "isolated route datum");
        AppendWorldMotif(favor_terms, "wind-scoured shelf");
        AppendWorldMotif(favor_terms, "apparently open ground");
        AppendWorldMotif(avoid_terms, "visible alien wall");
        AppendWorldMotif(avoid_terms, "dense machinery maze");
        AppendWorldMotif(avoid_terms, "sealed ceiling");
    }

    const char* gate_relation = DescribeSpatialGateRelation(spatial_state);
    if (strcmp(gate_relation, "baseward approach") == 0) {
        AppendWorldMotif(favor_terms, "split threshold gate");
        AppendWorldMotif(favor_terms, "base route placard");
        AppendWorldMotif(favor_terms, "survey cargo");
    } else if (strcmp(gate_relation, "east quarry flank") == 0) {
        AppendWorldMotif(favor_terms, "east survey beacon");
        AppendWorldMotif(favor_terms, "east retaining slab");
        AppendWorldMotif(favor_terms, "east datum pylon");
    } else if (strcmp(gate_relation, "west quarry flank") == 0) {
        AppendWorldMotif(favor_terms, "west sample case");
        AppendWorldMotif(favor_terms, "west route mark");
        AppendWorldMotif(favor_terms, "west quarry outcrop");
    } else if (strcmp(gate_relation, "deep field beyond the survey base") == 0) {
        AppendWorldMotif(favor_terms, "deep-field datum");
        AppendWorldMotif(favor_terms, "distant survey light");
        AppendWorldMotif(favor_terms, "long horizon cut");
    }

    static const char* const kOctantVariants[8][3] = {
        { "front route placard", "approach sample case", "front datum pylon" },
        { "angled survey beacon", "east-leaning cargo", "lateral route slate" },
        { "east crystal scanner", "east retaining fin", "east datum marker" },
        { "deep-east beacon", "crosswind instrument stand", "offset pylon" },
        { "distant survey light", "deep-field datum", "backline beacon" },
        { "deep-west outcrop", "shadowed sample case", "west rear marker" },
        { "west route placard", "west quarry pylon", "wind-shadow outcrop" },
        { "base-west buttress", "angled survey cargo", "west-front beacon" },
    };
    const int octant = ComputeSpatialOctant(spatial_state);
    const uint32_t seed = HashSpatialVariationSeed(spatial_state);
    AppendWorldMotif(favor_terms, kOctantVariants[octant][seed % 3u]);
}

static std::string BuildSpatialVariationGuideText(const SpatialState& spatial_state)
{
    std::string text;
    text += "Hidden Erycinian survey-field guide\n";
    AppendLabelValue(&text, "world_band: ", DescribeSpatialWorldBand(spatial_state));
    AppendLabelValue(&text, "survey_base_relation: ", DescribeSpatialGateRelation(spatial_state));
    AppendLabelValue(&text, "sky_exposure: ", DescribeSpatialSkyExposure(spatial_state));

    AppendLabelValue(&text, "radial_narrative: ", DescribeSpatialBandNarrative(spatial_state));
    AppendLabelValue(&text, "bearing_narrative: ", DescribeSpatialBearingNarrative(spatial_state));

    std::vector<std::string> favor_terms;
    std::vector<std::string> avoid_terms;
    BuildSpatialVariationMotifs(spatial_state, &favor_terms, &avoid_terms);
    AppendStringList(&text, "favor_terms: ", favor_terms);
    AppendStringList(&text, "avoid_terms: ", avoid_terms);
    text += "metadata_pressure: let title, location_archetype, anchors, visible_objects and scene_constraints echo at least two favor_terms.\n";
    text += "layout_pressure: prefer one dominant asymmetry and side-qualified constraints such as east pylon, deep-field beacon, west sample case or central scanner.\n";
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
    text += "    \"next_location_id\": \"quarry_threshold\" | \"extraction_field\" | \"crystal_cut\" | \"scanner_station\" | \"survey_plateau\" | \"labyrinth_threshold\" | \"prospect_shelter\" | \"unknown\",\n";
    text += "    \"move_count_changed\": boolean,\n";
    text += "    \"next_move_count\": integer,\n";
    text += "    \"score_changed\": boolean,\n";
    text += "    \"next_score\": integer,\n";
    text += "    \"alert_level_changed\": boolean,\n";
    text += "    \"next_alert_level\": integer,\n";
    text += "    \"spatial_entropy_changed\": boolean,\n";
    text += "    \"next_spatial_entropy\": integer,\n";
    text += "    \"external_temperature_changed\": boolean,\n";
    text += "    \"next_external_temperature_c\": integer,\n";
    text += "    \"body_temperature_changed\": boolean,\n";
    text += "    \"next_body_temperature_c\": number,\n";
    text += "    \"suit_state_changed\": boolean,\n";
    text += "    \"next_suit_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"oxygen_state_changed\": boolean,\n";
    text += "    \"next_oxygen_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"instrument_power_state_changed\": boolean,\n";
    text += "    \"next_instrument_power_state\": \"stable\" | \"strained\" | \"critical\" | \"unknown\",\n";
    text += "    \"inventory_add\": [string],\n";
    text += "    \"inventory_remove\": [string],\n";
    text += "    \"threats_add\": [string],\n";
    text += "    \"threats_remove\": [string]\n";
    text += "  },\n";
    text += "  \"spatial_delta\": {\n";
    text += "    \"location_changed\": boolean,\n";
    text += "    \"next_location_id\": \"quarry_threshold\" | \"extraction_field\" | \"crystal_cut\" | \"scanner_station\" | \"survey_plateau\" | \"labyrinth_threshold\" | \"prospect_shelter\" | \"unknown\",\n";
    text += "    \"time_of_day_changed\": boolean,\n";
    text += "    \"next_time_of_day\": \"day\" | \"dusk\" | \"night\" | \"unknown\",\n";
    text += "    \"visibility_changed\": boolean,\n";
    text += "    \"next_visibility_level\": \"clear\" | \"dusty\" | \"low\" | \"unknown\",\n";
    text += "    \"surface_weather_changed\": boolean,\n";
    text += "    \"next_surface_weather\": \"still\" | \"windy\" | \"dusty\" | \"unknown\",\n";
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
    text += "  \"next_spatial_entropy\": integer,\n";
    text += "  \"next_external_temperature_c\": integer,\n";
    text += "  \"next_body_temperature_c\": number,\n";
    text += "  \"spatial_state\": {\n";
    text += "    \"location_archetype\": string,\n";
    text += "    \"time_of_day\": \"day\" | \"dusk\" | \"night\" | \"unknown\",\n";
    text += "    \"visibility_level\": \"clear\" | \"dusty\" | \"low\" | \"unknown\",\n";
    text += "    \"surface_weather\": \"still\" | \"windy\" | \"dusty\" | \"unknown\",\n";
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
    text += "- active Eryx directives: room, camera, spotlight, sky, plane, box, prefab_gate, prefab_crate, prefab_survey_beacon, prefab_crystal_scanner, prefab_crystal_cluster, prefab_extraction_rig, prefab_prospect_shelter, prefab_quarry_pylon, prefab_atmospheric_processor, prefab_rock_low, prefab_rock_wide, prefab_rock_tall, prefab_rock_spire\n";
    text += "- every scene must declare one room and one camera\n";
    text += "- keep geometry sparse and legible\n";
    text += "- prefer stable repeated objects through prefab_* directives\n";
    text += "- avoid long decorative lists of tiny objects\n";
    text += "- keep the place readable at 800x400 palette-limited noisy rendering\n";
    text += "- stay inside the Venusian crystal-quarry fiction: brutalist extraction infrastructure, sparse hostile terrain, prospecting instruments, and invisible topology\n";
    text += "- use gray(), not color() or opacity()\n";
    text += "- gray() controls luminance only; the engine applies locked semantic colors to sky, terrain, crystals, and instrument lights\n";
    text += "- prefer names like ground, quarry_*, clay_*, ridge_* or outcrop_* for exterior terrain masses\n";
    text += "- valid examples:\n";
    text += "  room \"Erycinian crystal quarry survey shelf\"\n";
    text += "  camera eye(0.0,1.75,-7.9) target(0.0,1.18,8.8) up(0.0,1.0,0.0) fov(46.0)\n";
    text += "  spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(34.0) cone(12.0,28.0) intensity(70.0)\n";
    text += "  sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(77)\n";
    text += "  plane \"quarry_floor\" pos(0.0,0.0,-2.0) normal(0.0,1.0,0.0) size(16.0,24.0) gray(0.14)\n";
    text += "  box \"retaining_slab\" pos(0.0,0.55,7.0) size(14.0,1.1,0.7) gray(0.26)\n";
    text += "  prefab_crate \"sample_case\" pos(2.4,0.55,-1.8) size(1.7,1.1,1.4) gray(0.20) detail(0.34)\n";
    text += "  prefab_crystal_scanner \"affinity_scanner\" pos(-3.2,1.2,1.0) size(2.2,2.4,1.5) gray(0.25) detail(0.44) glow(0.28)\n";
    text += "  prefab_crystal_cluster \"exposed_vein\" pos(3.8,1.15,3.8) size(1.8,2.3,1.6) gray(0.66) glow(0.34)\n";
    text += "  prefab_survey_beacon \"route_datum\" pos(-5.0,1.7,6.8) size(1.1,3.4,1.0) gray(0.24) detail(0.43)\n";
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
    AppendLabelValue(&text, "surface_weather: ", DesertStateToString(spatial_state.desert_state));
    AppendLabelValue(&text, "interior_density: ", InteriorDensityToString(spatial_state.interior_density));
    AppendLabelValue(&text, "world_band: ", DescribeSpatialWorldBand(spatial_state));
    AppendLabelValue(&text, "survey_base_relation: ", DescribeSpatialGateRelation(spatial_state));
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
    GameLanguage language,
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
    text += "Stay within the Erycinian crystal-quarry expedition and its seven canonical places unless an unexplored cardinal exit explicitly requires one neighboring place.\n";
    if (spatial_state.location_id == kLocationUnknown) {
        text += "The current room may be an improvised generated room. Keep that room stable unless the player explicitly moves.\n";
    }
    text += "Narration must stay short, concrete, and spatially actionable.\n";
    text += "Write narration and clarification in idiomatic ";
    text += GameLanguageEnglishName(language);
    text += ". Keep JSON keys, IDs, intent labels, object names used as engine tokens, and all internal mechanics in English.\n";
    text += "Prefer 2 to 4 short sentences and stay roughly under 70 words.\n";
    text += "Avoid long atmospheric digressions and avoid restating the whole room summary.\n";
    text += "move_count tracks effectful actions only. Increase it only when the command materially changes position, inventory, state or knowledge.\n";
    text += "score should stay conservative and stable. Increase it only for meaningful progress, discoveries, access breakthroughs or solved obstacles.\n";
    text += "Whenever the room contains usable objects or interfaces, mention at least one concrete actionable affordance.\n";
    text += "Prefer nouns the player can survey, mark, scan, sample, inspect, open, read, take, or operate.\n";
    text += "Good actionable examples: survey beacon, quarry pylon, crystal scanner, crystal cluster, sample case, extraction rig, oxygen processor, shelter slate, route mark.\n";
    text += "If visible_objects are present in the spatial brief, reuse at least one of them directly in the narration when relevant.\n";
    text += "When the room topology is clear, keep spatial_delta.blocked_exits accurate so closed cardinal directions stay closed.\n";
    text += "If topology is clear, explicitly decide which cardinal directions are blocked instead of leaving the room ambiguous.\n";
    text += "When the decor composition becomes clearer or changes materially, update spatial_delta.scene_constraints with short layout cues, never coordinates.\n";
    text += "Good scene_constraints examples: split threshold masses, extraction rig on work slab, exposed crystal seam, scanner under cantilever, sparse beacon line, open horizon, no visible alien wall.\n";
    text += "spatial_entropy is a 0 to 100 confidence measure for accumulated topological contradiction. Raise it only when measurements, route marks, or returns cease to agree.\n";
    text += "external_temperature_c is the suit's measured Venus environment temperature. body_temperature_c is the wearer's core temperature. You decide whether either changes after the action. Keep them physically correlated but lagged: the exterior may fluctuate first, while the suit slows changes to the body. Shelter or atmospheric equipment may cool the body; open exposure, effort, suit damage, and time may heat it. Use small body changes, usually 0.0 to 0.4 C per turn.\n";
    text += "Invisible topology is never normal visible wall geometry. Describe bodily or instrument contact with apparently open space and preserve engine-owned traversal results.\n";
    text += "Suit, oxygen, and instrument power are hard resources. Change them only when the player's action materially affects them.\n";
    text += "\nCurrent hard state\n";
    AppendLabelValue(&text, "turn_number: ", std::to_string(hard_state.turn_number).c_str());
    AppendLabelValue(&text, "move_count: ", std::to_string(hard_state.move_count).c_str());
    AppendLabelValue(&text, "score: ", std::to_string(hard_state.score).c_str());
    AppendLabelValue(&text, "current_location_id: ", LocationIdToString(hard_state.current_location_id));
    AppendLabelValue(&text, "alert_level: ", std::to_string(hard_state.alert_level).c_str());
    AppendLabelValue(
        &text,
        "spatial_entropy: ",
        std::to_string(hard_state.spatial_entropy).c_str());
    AppendLabelValue(&text, "external_temperature_c: ", std::to_string(hard_state.external_temperature_c).c_str());
    AppendLabelValue(&text, "body_temperature_c: ", std::to_string(hard_state.body_temperature_c).c_str());
    AppendLabelValue(&text, "suit_state: ", ResourceStateToString(hard_state.cooling_state));
    AppendLabelValue(&text, "oxygen_state: ", ResourceStateToString(hard_state.water_state));
    AppendLabelValue(&text, "instrument_power_state: ", ResourceStateToString(hard_state.power_state));
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
    GameLanguage language,
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
    text += "The new place must stay inside the Erycinian Venus crystal-quarry expedition: human brutalist extraction works, hostile sparse terrain, and an alien topology that remains optically absent.\n";
    text += "It must be spatially readable, sparse, and suitable for grayscale raytracing.\n";
    text += "Do not describe a whole region. Describe one immediate neighboring room only.\n";
    text += "The reverse direction back to the source room should usually remain possible.\n";
    text += "Keep the title short.\n";
    text += "Keep the summary compact and concrete.\n";
    text += "Keep arrival_narration under about 60 words and make it immediately playable.\n";
    text += "Write title, summary, and arrival_narration in idiomatic ";
    text += GameLanguageEnglishName(language);
    text += ". Keep JSON keys, IDs, location_archetype, anchors, object tokens, constraints, and all internal mechanics in English.\n";
    text += "Set move_cost to 1 for a normal successful traversal, and use 0 only if the traversal has no meaningful effect.\n";
    text += "Set score_delta conservatively. Use 0 unless entering this room meaningfully advances progress or reveals something important.\n";
    text += "visible_objects must contain 3 to 5 concrete actionable objects or instruments, not vague scenery only.\n";
    text += "Include at least two directly usable things such as a survey beacon, quarry pylon, crystal scanner, crystal cluster, sample case, extraction rig, oxygen processor, shelter slate, or route mark.\n";
    text += "Use recurring human prefabs to make the expedition readable: gate, crate, survey beacon, crystal scanner, crystal cluster, extraction rig, prospect shelter, quarry pylon, and atmospheric processor.\n";
    text += "Never render the alien labyrinth as ordinary opaque walls, force fields, glowing grids, doors, or corridors. Its presence is inferred from contact, failed range data, displaced marks, and contradictory returns.\n";
    text += "scene_constraints should contain 1 to 4 short decor cues for the engine, describing dominant masses or layout bias, never coordinates.\n";
    text += "Good scene_constraints examples: split threshold masses, extraction rig on work slab, exposed crystal seam, scanner under cantilever, sparse beacon line, open horizon, no visible alien wall.\n";
    text += "spatial_entropy should shape title, summary, and arrival_narration through conflicting bearings, measurements, marks, or retraced paths, without turning it directly into visible geometry.\n";
    text += "Set next_spatial_entropy to the value that should remain after entering the place. Keep it close to the current value unless the traversal produces a concrete contradiction.\n";
    text += "Set next_external_temperature_c and next_body_temperature_c after arrival. You decide whether heat rises, falls, or remains stable. Keep them correlated but lagged: the suit buffers the body from Venusian exterior heat. Open exposure and exertion may heat the body; shelter or processors may cool it. Keep body changes small, usually 0.0 to 0.4 C per traversal.\n";
    text += "The engine maintains a hidden survey-field drift map. Treat the qualitative world cues below as ground truth.\n";
    text += "If the target world band suggests outer shelf or open venus, avoid inventing a sealed ceiling.\n";
    text += "If the target world band suggests survey camp or inner quarry, prefer compressed human works, quarry cuts, or partial shelter.\n";
    text += "At the quarry seam, exposed crystal, instrument decks, broken cuts, and partial shelter are plausible.\n";
    text += "When the target world band is open venus, use sparse route datums and hostile terrain rather than generic interior interfaces.\n";
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
        "spatial_entropy: ",
        std::to_string(hard_state.spatial_entropy).c_str());
    AppendLabelValue(&text, "external_temperature_c: ", std::to_string(hard_state.external_temperature_c).c_str());
    AppendLabelValue(&text, "body_temperature_c: ", std::to_string(hard_state.body_temperature_c).c_str());
    AppendLabelValue(&text, "suit_state: ", ResourceStateToString(hard_state.cooling_state));
    AppendLabelValue(&text, "oxygen_state: ", ResourceStateToString(hard_state.water_state));
    AppendLabelValue(&text, "instrument_power_state: ", ResourceStateToString(hard_state.power_state));
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
    text += "\nErycinian survey-field guide\n";
    text += BuildSpatialVariationGuideText(spatial_state);
    return text;
}

}  // namespace liminal
