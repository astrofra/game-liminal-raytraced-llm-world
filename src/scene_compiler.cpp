#include "scene_compiler.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace liminal {

namespace {

enum LayoutArchetype {
    kLayoutArchetypeUnknown = 0,
    kLayoutArchetypeQuarryThreshold,
    kLayoutArchetypeExtractionField,
    kLayoutArchetypeVenusPlateau,
    kLayoutArchetypeIndustrialServiceZone,
    kLayoutArchetypeProspectingShelter,
    kLayoutArchetypeScannerStation,
    kLayoutArchetypeLabyrinthThreshold,
    kLayoutArchetypeQuarryCut,
    kLayoutArchetypeThresholdExterior,
    kLayoutArchetypeRoofExterior,
    kLayoutArchetypeYardExterior,
    kLayoutArchetypeDesertExterior,
    kLayoutArchetypeServerAisles,
    kLayoutArchetypeControlHub,
    kLayoutArchetypeBackupVault,
    kLayoutArchetypeCoolingBay,
    kLayoutArchetypeServiceInterior,
};

enum ScenePrimitive {
    kScenePrimitiveBox = 0,
    kScenePrimitivePrefabGate,
    kScenePrimitivePrefabRack,
    kScenePrimitivePrefabCrate,
    kScenePrimitivePrefabCoolingUnit,
    kScenePrimitivePrefabAiServer,
    kScenePrimitivePrefabSurveyBeacon,
    kScenePrimitivePrefabCrystalScanner,
    kScenePrimitivePrefabCrystalCluster,
    kScenePrimitivePrefabExtractionRig,
    kScenePrimitivePrefabProspectShelter,
    kScenePrimitivePrefabQuarryPylon,
    kScenePrimitivePrefabAtmosphericProcessor,
    kScenePrimitivePrefabCactusSentinel,
    kScenePrimitivePrefabCactusFork,
    kScenePrimitivePrefabCactusCluster,
    kScenePrimitivePrefabRockLow,
    kScenePrimitivePrefabRockWide,
    kScenePrimitivePrefabRockTall,
    kScenePrimitivePrefabRockSpire,
};

enum LayoutMount {
    kLayoutMountFloor = 0,
    kLayoutMountWallLeft,
    kLayoutMountWallRight,
    kLayoutMountWallBack,
};

enum LayoutZone {
    kLayoutZoneLeft = 0,
    kLayoutZoneRight,
    kLayoutZoneCenter,
    kLayoutZoneFront,
    kLayoutZoneBack,
};

struct RoomShell {
    LayoutArchetype archetype;
    bool exterior;
    float half_width;
    float half_depth;
    float height;
    float corridor_half_width;
    float front_margin;
    float camera_eye_y;
    float camera_eye_z;
    float camera_target_y;
    float camera_target_z;
    float camera_fov;
    float spotlight_range;
    float spotlight_intensity;
    float floor_gray;
    float ceiling_gray;
    float wall_gray;
    float trim_gray;

    RoomShell()
        : archetype(kLayoutArchetypeUnknown)
        , exterior(false)
        , half_width(6.4f)
        , half_depth(11.5f)
        , height(3.2f)
        , corridor_half_width(1.2f)
        , front_margin(3.0f)
        , camera_eye_y(1.78f)
        , camera_eye_z(-7.8f)
        , camera_target_y(1.20f)
        , camera_target_z(8.4f)
        , camera_fov(46.0f)
        , spotlight_range(30.0f)
        , spotlight_intensity(78.0f)
        , floor_gray(0.12f)
        , ceiling_gray(0.19f)
        , wall_gray(0.18f)
        , trim_gray(0.24f)
    {
    }
};

struct LayoutObjectSpec {
    ScenePrimitive primitive;
    LayoutMount mount;
    LayoutZone zone;
    std::string name;
    Vec3 size;
    float gray;
    float detail;
    bool has_detail;
    bool emissive;
    float emit;
    int bars;
    bool blocks_corridor;
    int sequence_index;

    LayoutObjectSpec()
        : primitive(kScenePrimitiveBox)
        , mount(kLayoutMountFloor)
        , zone(kLayoutZoneCenter)
        , size(1.0f)
        , gray(0.24f)
        , detail(0.0f)
        , has_detail(false)
        , emissive(false)
        , emit(0.0f)
        , bars(0)
        , blocks_corridor(false)
        , sequence_index(0)
    {
    }
};

struct PlacedLayoutObject {
    LayoutObjectSpec spec;
    Vec3 pos;
};

static const char* LayoutArchetypeToString(LayoutArchetype archetype)
{
    switch (archetype) {
    case kLayoutArchetypeQuarryThreshold:
        return "quarry_threshold";
    case kLayoutArchetypeExtractionField:
        return "extraction_field";
    case kLayoutArchetypeVenusPlateau:
        return "venus_plateau";
    case kLayoutArchetypeIndustrialServiceZone:
        return "industrial_service_zone";
    case kLayoutArchetypeProspectingShelter:
        return "prospecting_shelter";
    case kLayoutArchetypeScannerStation:
        return "scanner_station";
    case kLayoutArchetypeLabyrinthThreshold:
        return "labyrinth_threshold";
    case kLayoutArchetypeQuarryCut:
        return "quarry_cut";
    case kLayoutArchetypeThresholdExterior:
        return "threshold_exterior";
    case kLayoutArchetypeRoofExterior:
        return "roof_exterior";
    case kLayoutArchetypeYardExterior:
        return "yard_exterior";
    case kLayoutArchetypeDesertExterior:
        return "desert_exterior";
    case kLayoutArchetypeServerAisles:
        return "server_aisles";
    case kLayoutArchetypeControlHub:
        return "control_hub";
    case kLayoutArchetypeBackupVault:
        return "backup_vault";
    case kLayoutArchetypeCoolingBay:
        return "cooling_bay";
    case kLayoutArchetypeServiceInterior:
        return "service_interior";
    default:
        return "unknown";
    }
}

static const char* ScenePrimitiveToString(ScenePrimitive primitive)
{
    switch (primitive) {
    case kScenePrimitiveBox:
        return "box";
    case kScenePrimitivePrefabGate:
        return "prefab_gate";
    case kScenePrimitivePrefabRack:
        return "prefab_rack";
    case kScenePrimitivePrefabCrate:
        return "prefab_crate";
    case kScenePrimitivePrefabCoolingUnit:
        return "prefab_cooling_unit";
    case kScenePrimitivePrefabAiServer:
        return "prefab_ai_server";
    case kScenePrimitivePrefabSurveyBeacon:
        return "prefab_survey_beacon";
    case kScenePrimitivePrefabCrystalScanner:
        return "prefab_crystal_scanner";
    case kScenePrimitivePrefabCrystalCluster:
        return "prefab_crystal_cluster";
    case kScenePrimitivePrefabExtractionRig:
        return "prefab_extraction_rig";
    case kScenePrimitivePrefabProspectShelter:
        return "prefab_prospect_shelter";
    case kScenePrimitivePrefabQuarryPylon:
        return "prefab_quarry_pylon";
    case kScenePrimitivePrefabAtmosphericProcessor:
        return "prefab_atmospheric_processor";
    case kScenePrimitivePrefabCactusSentinel:
        return "prefab_cactus_sentinel";
    case kScenePrimitivePrefabCactusFork:
        return "prefab_cactus_fork";
    case kScenePrimitivePrefabCactusCluster:
        return "prefab_cactus_cluster";
    case kScenePrimitivePrefabRockLow:
        return "prefab_rock_low";
    case kScenePrimitivePrefabRockWide:
        return "prefab_rock_wide";
    case kScenePrimitivePrefabRockTall:
        return "prefab_rock_tall";
    case kScenePrimitivePrefabRockSpire:
        return "prefab_rock_spire";
    default:
        return "unknown";
    }
}

static const char* LayoutMountToString(LayoutMount mount)
{
    switch (mount) {
    case kLayoutMountFloor:
        return "floor";
    case kLayoutMountWallLeft:
        return "wall_left";
    case kLayoutMountWallRight:
        return "wall_right";
    case kLayoutMountWallBack:
        return "wall_back";
    default:
        return "unknown";
    }
}

static const char* LayoutZoneToString(LayoutZone zone)
{
    switch (zone) {
    case kLayoutZoneLeft:
        return "left";
    case kLayoutZoneRight:
        return "right";
    case kLayoutZoneCenter:
        return "center";
    case kLayoutZoneFront:
        return "front";
    case kLayoutZoneBack:
        return "back";
    default:
        return "unknown";
    }
}

static void SetError(char* buffer, size_t buffer_size, const char* format, const char* argument)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, format, argument ? argument : "(null)");
}

static void SetErrorFormat(char* buffer, size_t buffer_size, const char* format, ...)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
}

static void AppendLine(std::string* text, const char* format, ...)
{
    if (!text) {
        return;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    text->append(buffer);
}

static bool ReadTextFile(const char* path, std::string* text, char* error_buffer, size_t error_buffer_size)
{
    if (!path || !text) {
        SetError(error_buffer, error_buffer_size, "Invalid text path: %s", "(null)");
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        SetError(error_buffer, error_buffer_size, "Cannot open text file: %s", path);
        return false;
    }

    text->clear();
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        text->append(buffer);
    }
    fclose(file);
    return true;
}

static const char* CanonicalFixturePath(LocationId location_id)
{
    switch (location_id) {
    case kLocationQuarryThreshold:
        return "assets/scenes/eryx_quarry_threshold.scene";
    case kLocationExtractionField:
        return "assets/scenes/eryx_extraction_field.scene";
    case kLocationCrystalCut:
        return "assets/scenes/eryx_crystal_cut.scene";
    case kLocationScannerStation:
        return "assets/scenes/eryx_scanner_station.scene";
    case kLocationSurveyPlateau:
        return "assets/scenes/eryx_survey_plateau.scene";
    case kLocationLabyrinthThreshold:
        return "assets/scenes/eryx_labyrinth_threshold.scene";
    case kLocationProspectShelter:
        return "assets/scenes/eryx_prospect_shelter.scene";
    case kLocationGate:
        return "assets/scenes/datacenter_entry_gate.scene";
    case kLocationServerAisles:
        return "assets/scenes/datacenter_server_aisles.scene";
    case kLocationRoofWatch:
        return "assets/scenes/datacenter_roof_watch.scene";
    default:
        return 0;
    }
}

static void SetCommonQuarryThresholdSpatialState(SpatialState* state)
{
    state->room_title = "Quarry Threshold";
    state->room_summary = "A split-crown pressure gate opens north onto the Erycinian quarry; a survey beacon and descending ramp establish the first reliable datum.";
    state->location_archetype = "quarry_threshold";
    state->canonical_fixture = CanonicalFixturePath(kLocationQuarryThreshold);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityDusty;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorSparse;
    state->alert_level = 1;
    state->anchors.push_back("split_crown_gate");
    state->anchors.push_back("datum_beacon_zero");
    state->anchors.push_back("descending_quarry_ramp");
    state->visible_objects.push_back("survey gate control");
    state->visible_objects.push_back("route beacon");
    state->visible_objects.push_back("sample cargo case");
    state->visible_objects.push_back("map slate cradle");
    state->blocked_exits.push_back("east");
    state->blocked_exits.push_back("south");
    state->blocked_exits.push_back("west");
    state->scene_constraints.push_back("hero quarry gate");
    state->scene_constraints.push_back("split brutalist crown");
    state->scene_constraints.push_back("survey beacon north");
    state->scene_constraints.push_back("open venus horizon");
}

static void SetCommonExtractionFieldSpatialState(SpatialState* state)
{
    state->room_title = "Extraction Field";
    state->room_summary = "A diamond drill stands among retaining fins and atmospheric machinery; the marked quarry cut continues north.";
    state->location_archetype = "extraction_field";
    state->canonical_fixture = CanonicalFixturePath(kLocationExtractionField);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityDusty;
    state->desert_state = kDesertWindy;
    state->interior_density = kInteriorSparse;
    state->alert_level = 1;
    state->anchors.push_back("diamond_drill");
    state->anchors.push_back("retaining_fins");
    state->anchors.push_back("atmospheric_processor");
    state->visible_objects.push_back("drill service lever");
    state->visible_objects.push_back("oxygen manifold");
    state->visible_objects.push_back("quarry datum pylon");
    state->visible_objects.push_back("sealed equipment crate");
    state->blocked_exits.push_back("east");
    state->blocked_exits.push_back("west");
    state->scene_constraints.push_back("hero extraction rig");
    state->scene_constraints.push_back("brutalist retaining fins");
    state->scene_constraints.push_back("atmospheric processor flank");
    state->scene_constraints.push_back("keep quarry ramp clear");
}

static void SetCommonCrystalCutSpatialState(SpatialState* state)
{
    state->room_title = "Crystal Cut";
    state->room_summary = "An exposed crystal vein interrupts a narrow excavation; the sample track turns east toward the affinity scanner.";
    state->location_archetype = "quarry_cut";
    state->canonical_fixture = CanonicalFixturePath(kLocationCrystalCut);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorSparse;
    state->alert_level = 2;
    state->anchors.push_back("exposed_crystal_vein");
    state->anchors.push_back("stepped_cut_wall");
    state->anchors.push_back("east_sample_track");
    state->visible_objects.push_back("loose crystal sample");
    state->visible_objects.push_back("sample case");
    state->visible_objects.push_back("vein marker");
    state->visible_objects.push_back("drill cutoff");
    state->blocked_exits.push_back("north");
    state->blocked_exits.push_back("west");
    state->scene_constraints.push_back("hero crystal cluster");
    state->scene_constraints.push_back("deep quarry cut");
    state->scene_constraints.push_back("east route clear");
    state->scene_constraints.push_back("faceted specimen glow");
}

static void SetCommonScannerStationSpatialState(SpatialState* state)
{
    state->room_title = "Affinity Scanner";
    state->room_summary = "A scanner hangs beneath a heavy cantilever with a reference crystal beneath it; the beacon route climbs north.";
    state->location_archetype = "scanner_station";
    state->canonical_fixture = CanonicalFixturePath(kLocationScannerStation);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityClear;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorSparse;
    state->alert_level = 2;
    state->anchors.push_back("scanner_cantilever");
    state->anchors.push_back("reference_crystal");
    state->anchors.push_back("north_beacon");
    state->visible_objects.push_back("affinity scanner controls");
    state->visible_objects.push_back("reference crystal");
    state->visible_objects.push_back("route comparison display");
    state->visible_objects.push_back("survey beacon access plate");
    state->blocked_exits.push_back("east");
    state->blocked_exits.push_back("south");
    state->scene_constraints.push_back("hero crystal scanner");
    state->scene_constraints.push_back("heavy cantilever frame");
    state->scene_constraints.push_back("reference crystal center");
    state->scene_constraints.push_back("north survey beacon");
}

static void SetCommonSurveyPlateauSpatialState(SpatialState* state)
{
    state->room_title = "Beacon Plateau";
    state->room_summary = "A sparse line of survey beacons crosses the plateau toward an eastern pair of pylons; every lamp agrees on distance but not direction.";
    state->location_archetype = "venus_plateau";
    state->canonical_fixture = CanonicalFixturePath(kLocationSurveyPlateau);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertWindy;
    state->interior_density = kInteriorSparse;
    state->alert_level = 3;
    state->anchors.push_back("beacon_line");
    state->anchors.push_back("split_horizon_mass");
    state->anchors.push_back("east_datum_pair");
    state->visible_objects.push_back("route beacon plate");
    state->visible_objects.push_back("survey mark post");
    state->visible_objects.push_back("wind-scoured sample crate");
    state->blocked_exits.push_back("north");
    state->blocked_exits.push_back("west");
    state->scene_constraints.push_back("hero survey beacon line");
    state->scene_constraints.push_back("east datum pylons");
    state->scene_constraints.push_back("minimal venus plateau");
    state->scene_constraints.push_back("open horizon");
}

static void SetCommonLabyrinthThresholdSpatialState(SpatialState* state)
{
    state->room_title = "Open Datum";
    state->room_summary = "Two quarry pylons frame an apparently open northern field; a suspended fragment and an interrupted scan trace imply contact with empty space.";
    state->location_archetype = "labyrinth_threshold";
    state->canonical_fixture = CanonicalFixturePath(kLocationLabyrinthThreshold);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityClear;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorSparse;
    state->alert_level = 4;
    state->anchors.push_back("paired_datum_pylons");
    state->anchors.push_back("suspended_fragment");
    state->anchors.push_back("eastern_shelter_beacon");
    state->visible_objects.push_back("boundary probe head");
    state->visible_objects.push_back("chalk datum marker");
    state->visible_objects.push_back("suspended crystal fragment");
    state->visible_objects.push_back("shelter route beacon");
    state->blocked_exits.push_back("south");
    state->spatial_anomalies.push_back("the northern scan line terminates without visible geometry");
    state->scene_constraints.push_back("hero labyrinth threshold");
    state->scene_constraints.push_back("paired quarry pylons");
    state->scene_constraints.push_back("suspended contact evidence");
    state->scene_constraints.push_back("no visible wall");
}

static void SetCommonProspectShelterSpatialState(SpatialState* state)
{
    state->room_title = "Vey Shelter";
    state->room_summary = "A low pressure shelter preserves Vey's equipment and route marks; its west hatch is logged as returning directly to the scanner.";
    state->location_archetype = "prospecting_shelter";
    state->canonical_fixture = CanonicalFixturePath(kLocationProspectShelter);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorDense;
    state->alert_level = 4;
    state->anchors.push_back("split_roof_shelter");
    state->anchors.push_back("oxygen_stacks");
    state->anchors.push_back("vey_route_map");
    state->visible_objects.push_back("Vey route recorder");
    state->visible_objects.push_back("scratched survey map");
    state->visible_objects.push_back("oxygen service manifold");
    state->visible_objects.push_back("sealed crystal sample case");
    state->blocked_exits.push_back("north");
    state->blocked_exits.push_back("east");
    state->blocked_exits.push_back("south");
    state->spatial_anomalies.push_back("the west return relation bypasses the open datum");
    state->scene_constraints.push_back("hero prospect shelter");
    state->scene_constraints.push_back("split brutalist roof");
    state->scene_constraints.push_back("atmospheric processor rear");
    state->scene_constraints.push_back("west hatch clear");
}

static void SetCommonGateSpatialState(SpatialState* state)
{
    state->room_title = "Entry Gate";
    state->room_summary = "A checkpoint threshold between the datacenter compound and the desert road, with controls and tagged equipment near the portal.";
    state->location_archetype = "entry_threshold";
    state->canonical_fixture = CanonicalFixturePath(kLocationGate);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityDusty;
    state->desert_state = kDesertDusty;
    state->interior_density = kInteriorSparse;
    state->alert_level = 1;
    state->anchors.push_back("portal");
    state->anchors.push_back("fence");
    state->anchors.push_back("service_road");
    state->visible_objects.push_back("badge reader");
    state->visible_objects.push_back("intercom panel");
    state->visible_objects.push_back("checkpoint crate");
    state->visible_objects.push_back("warning placard");
    state->visible_objects.push_back("sliding gate");
}

static void SetCommonAislesSpatialState(SpatialState* state)
{
    state->room_title = "Server Aisles";
    state->room_summary = "Dense rows of racks, maintenance lanes, cooling blocks and low night visibility, with service controls embedded in the aisle.";
    state->location_archetype = "dense_server_interior";
    state->canonical_fixture = CanonicalFixturePath(kLocationServerAisles);
    state->time_of_day = kTimeNight;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertStill;
    state->interior_density = kInteriorDense;
    state->alert_level = 2;
    state->anchors.push_back("server_aisles");
    state->anchors.push_back("maintenance_lane");
    state->anchors.push_back("cooling_blocks");
    state->visible_objects.push_back("rack access door");
    state->visible_objects.push_back("maintenance crate");
    state->visible_objects.push_back("cooling keypad");
    state->visible_objects.push_back("aisle placard");
    state->visible_objects.push_back("service panel");
}

static void SetCommonRoofSpatialState(SpatialState* state)
{
    state->room_title = "Roof Watch";
    state->room_summary = "A parapet walk above the datacenter with a dark horizon, exposed hardware and a dusty desert beyond.";
    state->location_archetype = "watch_post_exterior";
    state->canonical_fixture = CanonicalFixturePath(kLocationRoofWatch);
    state->time_of_day = kTimeDusk;
    state->visibility_level = kVisibilityLow;
    state->desert_state = kDesertDusty;
    state->interior_density = kInteriorSparse;
    state->alert_level = 2;
    state->anchors.push_back("parapet");
    state->anchors.push_back("horizon");
    state->anchors.push_back("roof_plant");
    state->visible_objects.push_back("roof hatch");
    state->visible_objects.push_back("maintenance lockbox");
    state->visible_objects.push_back("vent cutoff switch");
    state->visible_objects.push_back("warning beacon");
    state->visible_objects.push_back("service crate");
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

static bool ContainsSubstring(const std::string& lower_text, const char* pattern)
{
    return pattern && lower_text.find(pattern) != std::string::npos;
}

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

static bool LabelLooksInteriorOnly(const std::string& lower)
{
    return ContainsSubstring(lower, "ai server") ||
        ContainsSubstring(lower, "mainframe") ||
        ContainsSubstring(lower, "accelerator") ||
        ContainsSubstring(lower, "gpu") ||
        ContainsSubstring(lower, "tensor") ||
        ContainsSubstring(lower, "inference") ||
        ContainsSubstring(lower, "rack") ||
        ContainsSubstring(lower, "server") ||
        ContainsSubstring(lower, "pod") ||
        ContainsSubstring(lower, "cooling") ||
        ContainsSubstring(lower, "vent") ||
        ContainsSubstring(lower, "chiller") ||
        ContainsSubstring(lower, "hvac") ||
        ContainsSubstring(lower, "console") ||
        ContainsSubstring(lower, "pedestal") ||
        ContainsSubstring(lower, "desk") ||
        ContainsSubstring(lower, "cabinet") ||
        ContainsSubstring(lower, "locker") ||
        ContainsSubstring(lower, "service panel") ||
        ContainsSubstring(lower, "cooling keypad") ||
        ContainsSubstring(lower, "cabinet latch") ||
        ContainsSubstring(lower, "intercom") ||
        ContainsSubstring(lower, "badge reader") ||
        ContainsSubstring(lower, "reader");
}

static bool LabelLooksOpenExteriorSafe(const std::string& lower)
{
    return ContainsSubstring(lower, "gate") ||
        ContainsSubstring(lower, "door") ||
        ContainsSubstring(lower, "portal") ||
        ContainsSubstring(lower, "shutter") ||
        ContainsSubstring(lower, "barrier") ||
        ContainsSubstring(lower, "fence") ||
        ContainsSubstring(lower, "rail") ||
        ContainsSubstring(lower, "hatch") ||
        ContainsSubstring(lower, "crate") ||
        ContainsSubstring(lower, "box") ||
        ContainsSubstring(lower, "lockbox") ||
        ContainsSubstring(lower, "case") ||
        ContainsSubstring(lower, "spool") ||
        ContainsSubstring(lower, "cache") ||
        ContainsSubstring(lower, "beacon") ||
        ContainsSubstring(lower, "lamp") ||
        ContainsSubstring(lower, "light") ||
        ContainsSubstring(lower, "post") ||
        ContainsSubstring(lower, "mast") ||
        ContainsSubstring(lower, "pylon") ||
        ContainsSubstring(lower, "marker") ||
        ContainsSubstring(lower, "placard") ||
        ContainsSubstring(lower, "sign") ||
        ContainsSubstring(lower, "cactus") ||
        ContainsSubstring(lower, "rock") ||
        ContainsSubstring(lower, "ridge") ||
        ContainsSubstring(lower, "berm") ||
        ContainsSubstring(lower, "outcrop") ||
        ContainsSubstring(lower, "boulder") ||
        ContainsSubstring(lower, "crystal") ||
        ContainsSubstring(lower, "scanner") ||
        ContainsSubstring(lower, "drill") ||
        ContainsSubstring(lower, "extraction") ||
        ContainsSubstring(lower, "shelter") ||
        ContainsSubstring(lower, "atmospheric") ||
        ContainsSubstring(lower, "oxygen");
}

static bool ShouldSkipLabelForShell(const RoomShell& shell, const std::string& lower)
{
    if (!shell.exterior) {
        return false;
    }

    if (shell.archetype == kLayoutArchetypeThresholdExterior) {
        return false;
    }

    if (shell.archetype == kLayoutArchetypeDesertExterior) {
        return !LabelLooksOpenExteriorSafe(lower);
    }

    if (LabelLooksInteriorOnly(lower)) {
        return true;
    }

    return false;
}

static bool SpatialFeelsExterior(const SpatialState& state)
{
    if (SpatialFeelsOpenDesert(state) || SpatialFeelsOuterParapet(state)) {
        return true;
    }

    const std::string combined = BuildSpatialSemanticText(state);
    const bool hard_exterior = ContainsSubstring(combined, "exterior") ||
        ContainsSubstring(combined, "roof") ||
        ContainsSubstring(combined, "yard") ||
        ContainsSubstring(combined, "desert") ||
        ContainsSubstring(combined, "watch") ||
        ContainsSubstring(combined, "parapet") ||
        ContainsSubstring(combined, "horizon") ||
        ContainsSubstring(combined, "sky") ||
        ContainsSubstring(combined, "outside") ||
        ContainsSubstring(combined, "quarry") ||
        ContainsSubstring(combined, "plateau") ||
        ContainsSubstring(combined, "extraction field") ||
        ContainsSubstring(combined, "labyrinth threshold") ||
        ContainsSubstring(combined, "venus");
    if (hard_exterior) {
        return true;
    }

    const bool explicit_interior = ContainsSubstring(combined, "interior") ||
        ContainsSubstring(combined, "control") ||
        ContainsSubstring(combined, "hub") ||
        ContainsSubstring(combined, "vault") ||
        ContainsSubstring(combined, "aisle") ||
        ContainsSubstring(combined, "server") ||
        ContainsSubstring(combined, "junction") ||
        ContainsSubstring(combined, "alcove") ||
        ContainsSubstring(combined, "room") ||
        ContainsSubstring(combined, "bay") ||
        ContainsSubstring(combined, "chamber");
    if (explicit_interior) {
        return false;
    }

    return ContainsSubstring(combined, "gate") ||
        ContainsSubstring(combined, "perimeter") ||
        ContainsSubstring(combined, "threshold");
}

static uint32_t HashTextSeed(const std::string& text)
{
    uint32_t hash = 2166136261u;
    for (size_t index = 0; index < text.size(); ++index) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(text[index]));
        hash *= 16777619u;
    }
    return hash ? hash : 1u;
}

static LayoutArchetype InferLayoutArchetype(const SpatialState& spatial_state)
{
    const std::string combined = BuildSpatialSemanticText(spatial_state);
    const bool exterior = SpatialFeelsExterior(spatial_state);

    if (ContainsSubstring(combined, "quarry_threshold") || ContainsSubstring(combined, "quarry threshold")) {
        return kLayoutArchetypeQuarryThreshold;
    }
    if (ContainsSubstring(combined, "extraction_field") || ContainsSubstring(combined, "extraction field")) {
        return kLayoutArchetypeExtractionField;
    }
    if (ContainsSubstring(combined, "venus_plateau") || ContainsSubstring(combined, "venus plateau") ||
        ContainsSubstring(combined, "survey plateau")) {
        return kLayoutArchetypeVenusPlateau;
    }
    if (ContainsSubstring(combined, "industrial_service_zone") || ContainsSubstring(combined, "industrial service zone")) {
        return kLayoutArchetypeIndustrialServiceZone;
    }
    if (ContainsSubstring(combined, "prospecting_shelter") || ContainsSubstring(combined, "prospecting shelter") ||
        ContainsSubstring(combined, "prospect shelter")) {
        return kLayoutArchetypeProspectingShelter;
    }
    if (ContainsSubstring(combined, "scanner_station") || ContainsSubstring(combined, "scanner station") ||
        ContainsSubstring(combined, "affinity scanner")) {
        return kLayoutArchetypeScannerStation;
    }
    if (ContainsSubstring(combined, "labyrinth_threshold") || ContainsSubstring(combined, "labyrinth threshold") ||
        ContainsSubstring(combined, "open datum")) {
        return kLayoutArchetypeLabyrinthThreshold;
    }
    if (ContainsSubstring(combined, "quarry_cut") || ContainsSubstring(combined, "quarry cut") ||
        ContainsSubstring(combined, "crystal cut")) {
        return kLayoutArchetypeQuarryCut;
    }

    if (SpatialFeelsOpenDesert(spatial_state) ||
        ContainsSubstring(combined, "cactus") ||
        (ContainsSubstring(combined, "rock") && ContainsSubstring(combined, "desert"))) {
        return kLayoutArchetypeDesertExterior;
    }

    if (ContainsSubstring(combined, "server") || ContainsSubstring(combined, "aisle") || ContainsSubstring(combined, "rack")) {
        return kLayoutArchetypeServerAisles;
    }
    if (ContainsSubstring(combined, "cooling") || ContainsSubstring(combined, "exchange") || ContainsSubstring(combined, "chiller")) {
        return kLayoutArchetypeCoolingBay;
    }
    if (ContainsSubstring(combined, "vault") || ContainsSubstring(combined, "archive") || ContainsSubstring(combined, "backup")) {
        return kLayoutArchetypeBackupVault;
    }
    if (ContainsSubstring(combined, "control") || ContainsSubstring(combined, "console") || ContainsSubstring(combined, "hub")) {
        return kLayoutArchetypeControlHub;
    }
    if (ContainsSubstring(combined, "roof") || ContainsSubstring(combined, "parapet") || ContainsSubstring(combined, "watch")) {
        return kLayoutArchetypeRoofExterior;
    }
    if (SpatialFeelsOuterParapet(spatial_state) || SpatialFeelsPerimeterSeam(spatial_state)) {
        return kLayoutArchetypeRoofExterior;
    }
    if (exterior && (ContainsSubstring(combined, "gate") || ContainsSubstring(combined, "checkpoint") || ContainsSubstring(combined, "threshold") ||
            ContainsSubstring(combined, "vestibule") || ContainsSubstring(combined, "sally"))) {
        return kLayoutArchetypeThresholdExterior;
    }
    if (exterior) {
        return kLayoutArchetypeYardExterior;
    }
    return kLayoutArchetypeServiceInterior;
}

static RoomShell BuildRoomShell(const SpatialState& spatial_state)
{
    RoomShell shell;
    shell.exterior = SpatialFeelsExterior(spatial_state);
    shell.archetype = InferLayoutArchetype(spatial_state);

    switch (shell.archetype) {
    case kLayoutArchetypeQuarryThreshold:
        shell.exterior = true;
        shell.half_width = 9.6f;
        shell.half_depth = 14.5f;
        shell.height = 5.0f;
        shell.corridor_half_width = 2.5f;
        shell.front_margin = 4.4f;
        shell.camera_eye_y = 1.82f;
        shell.camera_eye_z = -9.2f;
        shell.camera_target_y = 1.25f;
        shell.camera_target_z = 8.8f;
        shell.camera_fov = 48.0f;
        shell.spotlight_range = 36.0f;
        shell.spotlight_intensity = 70.0f;
        shell.floor_gray = 0.13f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.27f;
        break;
    case kLayoutArchetypeExtractionField:
        shell.exterior = true;
        shell.half_width = 11.5f;
        shell.half_depth = 17.0f;
        shell.height = 5.2f;
        shell.corridor_half_width = 2.8f;
        shell.front_margin = 3.8f;
        shell.camera_eye_y = 1.86f;
        shell.camera_eye_z = -10.0f;
        shell.camera_target_y = 1.30f;
        shell.camera_target_z = 10.5f;
        shell.camera_fov = 49.0f;
        shell.spotlight_range = 40.0f;
        shell.spotlight_intensity = 66.0f;
        shell.floor_gray = 0.13f;
        shell.wall_gray = 0.19f;
        shell.trim_gray = 0.26f;
        break;
    case kLayoutArchetypeVenusPlateau:
    case kLayoutArchetypeLabyrinthThreshold:
        shell.exterior = true;
        shell.half_width = 13.0f;
        shell.half_depth = 19.0f;
        shell.height = 4.5f;
        shell.corridor_half_width = 0.0f;
        shell.front_margin = 3.2f;
        shell.camera_eye_y = 1.82f;
        shell.camera_eye_z = -10.4f;
        shell.camera_target_y = 1.15f;
        shell.camera_target_z = 12.0f;
        shell.camera_fov = 50.0f;
        shell.spotlight_range = 42.0f;
        shell.spotlight_intensity = shell.archetype == kLayoutArchetypeLabyrinthThreshold ? 58.0f : 62.0f;
        shell.floor_gray = 0.12f;
        shell.wall_gray = 0.17f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeIndustrialServiceZone:
        shell.exterior = false;
        shell.half_width = 7.0f;
        shell.half_depth = 12.0f;
        shell.height = 4.0f;
        shell.corridor_half_width = 1.8f;
        shell.front_margin = 3.5f;
        shell.camera_eye_y = 1.80f;
        shell.camera_eye_z = -8.0f;
        shell.camera_target_y = 1.22f;
        shell.camera_target_z = 8.5f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 31.0f;
        shell.spotlight_intensity = 76.0f;
        shell.floor_gray = 0.11f;
        shell.ceiling_gray = 0.18f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeProspectingShelter:
        shell.exterior = false;
        shell.half_width = 6.2f;
        shell.half_depth = 10.0f;
        shell.height = 3.4f;
        shell.corridor_half_width = 1.5f;
        shell.front_margin = 3.2f;
        shell.camera_eye_y = 1.72f;
        shell.camera_eye_z = -7.1f;
        shell.camera_target_y = 1.18f;
        shell.camera_target_z = 6.8f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 27.0f;
        shell.spotlight_intensity = 82.0f;
        shell.floor_gray = 0.10f;
        shell.ceiling_gray = 0.17f;
        shell.wall_gray = 0.19f;
        shell.trim_gray = 0.27f;
        break;
    case kLayoutArchetypeScannerStation:
        shell.exterior = true;
        shell.half_width = 9.8f;
        shell.half_depth = 15.0f;
        shell.height = 5.0f;
        shell.corridor_half_width = 2.2f;
        shell.front_margin = 4.0f;
        shell.camera_eye_y = 1.80f;
        shell.camera_eye_z = -9.2f;
        shell.camera_target_y = 1.25f;
        shell.camera_target_z = 8.8f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 36.0f;
        shell.spotlight_intensity = 74.0f;
        shell.floor_gray = 0.12f;
        shell.wall_gray = 0.19f;
        shell.trim_gray = 0.28f;
        break;
    case kLayoutArchetypeQuarryCut:
        shell.exterior = true;
        shell.half_width = 8.0f;
        shell.half_depth = 17.0f;
        shell.height = 5.8f;
        shell.corridor_half_width = 2.0f;
        shell.front_margin = 3.8f;
        shell.camera_eye_y = 1.78f;
        shell.camera_eye_z = -9.8f;
        shell.camera_target_y = 1.35f;
        shell.camera_target_z = 10.0f;
        shell.camera_fov = 48.0f;
        shell.spotlight_range = 38.0f;
        shell.spotlight_intensity = 72.0f;
        shell.floor_gray = 0.12f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeThresholdExterior:
        shell.exterior = true;
        shell.half_width = 8.6f;
        shell.half_depth = 14.0f;
        shell.height = 4.4f;
        shell.corridor_half_width = 2.2f;
        shell.front_margin = 4.4f;
        shell.camera_eye_y = 1.82f;
        shell.camera_eye_z = -8.9f;
        shell.camera_target_y = 1.30f;
        shell.camera_target_z = 8.8f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 34.0f;
        shell.spotlight_intensity = 70.0f;
        shell.floor_gray = 0.13f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeRoofExterior:
        shell.exterior = true;
        shell.half_width = 8.8f;
        shell.half_depth = 15.5f;
        shell.height = 4.0f;
        shell.corridor_half_width = 2.0f;
        shell.front_margin = 4.0f;
        shell.camera_eye_y = 1.90f;
        shell.camera_eye_z = -9.4f;
        shell.camera_target_y = 1.20f;
        shell.camera_target_z = 9.8f;
        shell.camera_fov = 48.0f;
        shell.spotlight_range = 34.0f;
        shell.spotlight_intensity = 64.0f;
        shell.floor_gray = 0.13f;
        shell.wall_gray = 0.21f;
        shell.trim_gray = 0.27f;
        break;
    case kLayoutArchetypeYardExterior:
        shell.exterior = true;
        shell.half_width = 9.8f;
        shell.half_depth = 15.0f;
        shell.height = 4.2f;
        shell.corridor_half_width = 2.4f;
        shell.front_margin = 4.2f;
        shell.camera_eye_y = 1.84f;
        shell.camera_eye_z = -9.0f;
        shell.camera_target_y = 1.22f;
        shell.camera_target_z = 8.6f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 36.0f;
        shell.spotlight_intensity = 72.0f;
        shell.floor_gray = 0.12f;
        shell.wall_gray = 0.19f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeDesertExterior:
        shell.exterior = true;
        shell.half_width = 11.5f;
        shell.half_depth = 17.0f;
        shell.height = 3.8f;
        shell.corridor_half_width = 0.0f;
        shell.front_margin = 3.6f;
        shell.camera_eye_y = 1.82f;
        shell.camera_eye_z = -9.8f;
        shell.camera_target_y = 1.20f;
        shell.camera_target_z = 10.6f;
        shell.camera_fov = 48.0f;
        shell.spotlight_range = 36.0f;
        shell.spotlight_intensity = 60.0f;
        shell.floor_gray = 0.13f;
        shell.wall_gray = 0.17f;
        shell.trim_gray = 0.23f;
        break;
    case kLayoutArchetypeServerAisles:
        shell.exterior = false;
        shell.half_width = 6.8f;
        shell.half_depth = 14.0f;
        shell.height = 3.3f;
        shell.corridor_half_width = 1.6f;
        shell.front_margin = 3.6f;
        shell.camera_eye_y = 1.78f;
        shell.camera_eye_z = -8.1f;
        shell.camera_target_y = 1.20f;
        shell.camera_target_z = 9.6f;
        shell.camera_fov = 46.0f;
        shell.spotlight_range = 30.0f;
        shell.spotlight_intensity = 80.0f;
        shell.floor_gray = 0.12f;
        shell.ceiling_gray = 0.19f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.24f;
        break;
    case kLayoutArchetypeControlHub:
        shell.exterior = false;
        shell.half_width = 5.8f;
        shell.half_depth = 10.4f;
        shell.height = 3.1f;
        shell.corridor_half_width = 1.4f;
        shell.front_margin = 3.2f;
        shell.camera_eye_y = 1.76f;
        shell.camera_eye_z = -7.2f;
        shell.camera_target_y = 1.16f;
        shell.camera_target_z = 7.0f;
        shell.camera_fov = 46.0f;
        shell.spotlight_range = 28.0f;
        shell.spotlight_intensity = 78.0f;
        shell.floor_gray = 0.12f;
        shell.ceiling_gray = 0.20f;
        shell.wall_gray = 0.19f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeBackupVault:
        shell.exterior = false;
        shell.half_width = 6.2f;
        shell.half_depth = 11.4f;
        shell.height = 3.2f;
        shell.corridor_half_width = 1.5f;
        shell.front_margin = 3.4f;
        shell.camera_eye_y = 1.78f;
        shell.camera_eye_z = -7.6f;
        shell.camera_target_y = 1.18f;
        shell.camera_target_z = 8.2f;
        shell.camera_fov = 46.0f;
        shell.spotlight_range = 29.0f;
        shell.spotlight_intensity = 76.0f;
        shell.floor_gray = 0.12f;
        shell.ceiling_gray = 0.19f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.25f;
        break;
    case kLayoutArchetypeCoolingBay:
        shell.exterior = false;
        shell.half_width = 7.6f;
        shell.half_depth = 13.0f;
        shell.height = 4.6f;
        shell.corridor_half_width = 2.0f;
        shell.front_margin = 3.8f;
        shell.camera_eye_y = 1.88f;
        shell.camera_eye_z = -8.4f;
        shell.camera_target_y = 1.40f;
        shell.camera_target_z = 9.4f;
        shell.camera_fov = 47.0f;
        shell.spotlight_range = 34.0f;
        shell.spotlight_intensity = 82.0f;
        shell.floor_gray = 0.12f;
        shell.ceiling_gray = 0.17f;
        shell.wall_gray = 0.17f;
        shell.trim_gray = 0.23f;
        break;
    case kLayoutArchetypeServiceInterior:
    default:
        shell.exterior = false;
        shell.half_width = 6.4f;
        shell.half_depth = 11.5f;
        shell.height = 3.2f;
        shell.corridor_half_width = 1.4f;
        shell.front_margin = 3.3f;
        shell.camera_eye_y = 1.78f;
        shell.camera_eye_z = -7.8f;
        shell.camera_target_y = 1.20f;
        shell.camera_target_z = 8.4f;
        shell.camera_fov = 46.0f;
        shell.spotlight_range = 30.0f;
        shell.spotlight_intensity = 78.0f;
        shell.floor_gray = 0.12f;
        shell.ceiling_gray = 0.19f;
        shell.wall_gray = 0.18f;
        shell.trim_gray = 0.24f;
        break;
    }

    return shell;
}

static std::string SanitizeIdentifier(const std::string& label, const char* fallback_prefix, int index)
{
    std::string identifier;
    const std::string lower = ToLowerAsciiCopy(label);
    for (size_t i = 0; i < lower.size(); ++i) {
        const char value = lower[i];
        const bool is_letter = value >= 'a' && value <= 'z';
        const bool is_digit = value >= '0' && value <= '9';
        if (is_letter || is_digit) {
            identifier.push_back(value);
        } else if (!identifier.empty() && identifier[identifier.size() - 1] != '_') {
            identifier.push_back('_');
        }
    }

    while (!identifier.empty() && identifier[identifier.size() - 1] == '_') {
        identifier.resize(identifier.size() - 1);
    }

    if (identifier.empty()) {
        identifier = fallback_prefix ? fallback_prefix : "obj";
    }

    if (!(identifier[0] >= 'a' && identifier[0] <= 'z')) {
        identifier.insert(identifier.begin(), 'o');
        identifier.insert(identifier.begin() + 1, '_');
    }

    char suffix[32];
    snprintf(suffix, sizeof(suffix), "_%02d", index + 1);
    identifier.append(suffix);
    return identifier;
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

static void AddObjectSpec(std::vector<LayoutObjectSpec>* specs, const LayoutObjectSpec& spec)
{
    if (!specs) {
        return;
    }

    LayoutObjectSpec resolved = spec;
    resolved.sequence_index = static_cast<int>(specs->size());
    specs->push_back(resolved);
}

static void AddWallControlSpec(
    std::vector<LayoutObjectSpec>* specs,
    const std::string& label,
    LayoutMount mount,
    LayoutZone zone,
    int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitiveBox;
    spec.mount = mount;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "panel", index);
    spec.size = mount == kLayoutMountWallBack ? Vec3(0.9f, 0.65f, 0.14f) : Vec3(0.14f, 0.65f, 0.9f);
    spec.gray = 0.39f;
    AddObjectSpec(specs, spec);
}

static void AddBeaconSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitiveBox;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "beacon", index);
    spec.size = Vec3(0.38f, 2.3f, 0.38f);
    spec.gray = 0.30f;
    spec.emissive = true;
    spec.emit = 2.4f;
    AddObjectSpec(specs, spec);
}

static void AddGateSpec(std::vector<LayoutObjectSpec>* specs, const RoomShell& shell, const std::string& label, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitivePrefabGate;
    spec.mount = kLayoutMountWallBack;
    spec.zone = kLayoutZoneCenter;
    spec.name = SanitizeIdentifier(label, "gate", index);
    spec.size = shell.exterior ? Vec3(4.6f, 2.9f, 0.42f) : Vec3(2.2f, 2.5f, 0.28f);
    spec.gray = shell.exterior ? 0.30f : 0.23f;
    spec.has_detail = true;
    spec.detail = 0.42f;
    spec.bars = shell.exterior ? 5 : 0;
    AddObjectSpec(specs, spec);
}

static void AddRackSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitivePrefabRack;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "rack", index);
    spec.size = Vec3(1.35f, 2.50f, 1.70f);
    spec.gray = 0.20f;
    spec.has_detail = true;
    spec.detail = 0.35f;
    spec.blocks_corridor = true;
    AddObjectSpec(specs, spec);
}

static void AddCoolingSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitivePrefabCoolingUnit;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "cooling", index);
    spec.size = Vec3(1.20f, 2.35f, 1.20f);
    spec.gray = 0.29f;
    spec.has_detail = true;
    spec.detail = 0.38f;
    spec.blocks_corridor = true;
    AddObjectSpec(specs, spec);
}

static void AddAiServerSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitivePrefabAiServer;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "ai_server", index);
    spec.size = Vec3(1.70f, 2.80f, 1.70f);
    spec.gray = 0.16f;
    spec.has_detail = true;
    spec.detail = 0.28f;
    spec.blocks_corridor = true;
    AddObjectSpec(specs, spec);
}

static void AddCactusSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    const int variant = index % 3;
    spec.primitive =
        variant == 0 ? kScenePrimitivePrefabCactusSentinel
        : (variant == 1 ? kScenePrimitivePrefabCactusFork : kScenePrimitivePrefabCactusCluster);
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "cactus", index);
    spec.size = variant == 2 ? Vec3(1.20f, 2.10f, 1.20f) : Vec3(1.10f, 2.45f, 1.10f);
    spec.gray = 0.25f;
    spec.blocks_corridor = false;
    AddObjectSpec(specs, spec);
}

static void AddRockSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    const int variant = index % 4;
    spec.primitive =
        variant == 0 ? kScenePrimitivePrefabRockLow
        : (variant == 1 ? kScenePrimitivePrefabRockWide
        : (variant == 2 ? kScenePrimitivePrefabRockTall : kScenePrimitivePrefabRockSpire));
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "rock", index);
    spec.size =
        variant == 0 ? Vec3(1.60f, 1.00f, 1.30f)
        : (variant == 1 ? Vec3(2.10f, 1.20f, 1.70f)
        : (variant == 2 ? Vec3(1.30f, 1.90f, 1.20f) : Vec3(1.00f, 2.30f, 0.95f)));
    spec.gray = 0.23f;
    spec.blocks_corridor = false;
    AddObjectSpec(specs, spec);
}

static void AddCrateSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitivePrefabCrate;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "crate", index);
    spec.size = Vec3(1.70f, 1.10f, 1.40f);
    spec.gray = 0.20f;
    spec.has_detail = true;
    spec.detail = 0.31f;
    AddObjectSpec(specs, spec);
}

static void AddEryxPrefabSpec(
    std::vector<LayoutObjectSpec>* specs,
    ScenePrimitive primitive,
    const std::string& label,
    const char* fallback_name,
    LayoutZone zone,
    int index,
    const Vec3& size,
    float gray,
    float detail,
    bool blocks_corridor)
{
    LayoutObjectSpec spec;
    spec.primitive = primitive;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, fallback_name, index);
    spec.size = size;
    spec.gray = gray;
    spec.has_detail = detail > 0.0f;
    spec.detail = detail;
    spec.blocks_corridor = blocks_corridor;
    AddObjectSpec(specs, spec);
}

static void AddSurveyBeaconSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabSurveyBeacon,
        label,
        "survey_beacon",
        zone,
        index,
        Vec3(1.10f, 3.40f, 1.00f),
        0.24f,
        0.43f,
        false);
}

static void AddCrystalScannerSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabCrystalScanner,
        label,
        "crystal_scanner",
        zone,
        index,
        Vec3(2.20f, 2.40f, 1.50f),
        0.25f,
        0.44f,
        true);
}

static void AddCrystalClusterSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabCrystalCluster,
        label,
        "crystal_cluster",
        zone,
        index,
        Vec3(1.80f, 2.30f, 1.60f),
        0.66f,
        0.0f,
        false);
}

static void AddExtractionRigSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabExtractionRig,
        label,
        "extraction_rig",
        zone,
        index,
        Vec3(2.80f, 3.20f, 2.20f),
        0.23f,
        0.42f,
        true);
}

static void AddProspectShelterSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabProspectShelter,
        label,
        "prospect_shelter",
        zone,
        index,
        Vec3(4.80f, 3.10f, 3.50f),
        0.28f,
        0.40f,
        true);
}

static void AddQuarryPylonSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabQuarryPylon,
        label,
        "quarry_pylon",
        zone,
        index,
        Vec3(1.40f, 3.50f, 1.20f),
        0.27f,
        0.44f,
        false);
}

static void AddAtmosphericProcessorSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    AddEryxPrefabSpec(
        specs,
        kScenePrimitivePrefabAtmosphericProcessor,
        label,
        "atmospheric_processor",
        zone,
        index,
        Vec3(2.20f, 3.00f, 1.70f),
        0.24f,
        0.40f,
        true);
}

static void AddConsoleSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitiveBox;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "console", index);
    spec.size = Vec3(1.7f, 1.0f, 0.8f);
    spec.gray = 0.25f;
    AddObjectSpec(specs, spec);
}

static void AddHatchSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitiveBox;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "hatch", index);
    spec.size = Vec3(1.8f, 0.24f, 1.8f);
    spec.gray = 0.22f;
    AddObjectSpec(specs, spec);
}

static void AddLockerSpec(std::vector<LayoutObjectSpec>* specs, const std::string& label, LayoutZone zone, int index)
{
    LayoutObjectSpec spec;
    spec.primitive = kScenePrimitiveBox;
    spec.mount = kLayoutMountFloor;
    spec.zone = zone;
    spec.name = SanitizeIdentifier(label, "locker", index);
    spec.size = Vec3(1.1f, 2.0f, 0.9f);
    spec.gray = 0.24f;
    spec.blocks_corridor = true;
    AddObjectSpec(specs, spec);
}

static bool HasPrimitive(const std::vector<LayoutObjectSpec>& specs, ScenePrimitive primitive);

static void BuildVisibleObjectSpecs(
    const SpatialState& spatial_state,
    const RoomShell& shell,
    std::vector<LayoutObjectSpec>* specs)
{
    if (!specs) {
        return;
    }

    int wall_controls = 0;
    int floor_objects = 0;
    int rack_objects = 0;
    int cooling_objects = 0;
    int ai_server_objects = 0;
    int crate_objects = 0;
    int gate_objects = 0;
    int cactus_objects = 0;
    int rock_objects = 0;

    for (size_t index = 0; index < spatial_state.visible_objects.size(); ++index) {
        const std::string label = spatial_state.visible_objects[index];
        const std::string lower = ToLowerAsciiCopy(label);

        if (floor_objects >= 9 && wall_controls >= 5) {
            break;
        }

        if (ShouldSkipLabelForShell(shell, lower)) {
            continue;
        }

        if (ContainsSubstring(lower, "crystal scanner") || ContainsSubstring(lower, "crystal detector") ||
            ContainsSubstring(lower, "sample instrument") || ContainsSubstring(lower, "affinity detector")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalScanner) && floor_objects < 10) {
                AddCrystalScannerSpec(specs, label, kLayoutZoneCenter, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "drill") || ContainsSubstring(lower, "extraction rig") ||
            ContainsSubstring(lower, "boring rig") || ContainsSubstring(lower, "mining rig")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabExtractionRig) && floor_objects < 10) {
                AddExtractionRigSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "prospect shelter") || ContainsSubstring(lower, "prospecting station") ||
            ContainsSubstring(lower, "field shelter") || ContainsSubstring(lower, "survey shelter")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabProspectShelter) && floor_objects < 10) {
                AddProspectShelterSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "atmospheric processor") || ContainsSubstring(lower, "air processor") ||
            ContainsSubstring(lower, "oxygen service") || ContainsSubstring(lower, "electrolyser") ||
            ContainsSubstring(lower, "extraction pump")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabAtmosphericProcessor) && floor_objects < 10) {
                AddAtmosphericProcessorSpec(specs, label, kLayoutZoneLeft, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "quarry pylon") || ContainsSubstring(lower, "datum pylon") ||
            ContainsSubstring(lower, "industrial pylon")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon) && floor_objects < 10) {
                AddQuarryPylonSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "survey beacon") || ContainsSubstring(lower, "navigation mast") ||
            ContainsSubstring(lower, "range beacon")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon) && floor_objects < 10) {
                AddSurveyBeaconSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "crystal")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalCluster) && floor_objects < 10) {
                AddCrystalClusterSpec(specs, label, kLayoutZoneCenter, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "reader") || ContainsSubstring(lower, "keypad") || ContainsSubstring(lower, "intercom") ||
            ContainsSubstring(lower, "switch") || ContainsSubstring(lower, "placard") || ContainsSubstring(lower, "panel") ||
            ContainsSubstring(lower, "board") || ContainsSubstring(lower, "display") ||
            ContainsSubstring(lower, "sign") || ContainsSubstring(lower, "latch")) {
            if (wall_controls < 5) {
                const LayoutMount mount = (wall_controls % 3 == 0)
                    ? kLayoutMountWallBack
                    : ((wall_controls % 2 == 0) ? kLayoutMountWallRight : kLayoutMountWallLeft);
                const LayoutZone zone = wall_controls % 2 == 0 ? kLayoutZoneBack : kLayoutZoneCenter;
                AddWallControlSpec(specs, label, mount, zone, static_cast<int>(index));
                ++wall_controls;
            }
            continue;
        }

        if (ContainsSubstring(lower, "beacon") || ContainsSubstring(lower, "lamp") || ContainsSubstring(lower, "light") ||
            ContainsSubstring(lower, "post") || ContainsSubstring(lower, "mast") || ContainsSubstring(lower, "pylon") ||
            ContainsSubstring(lower, "marker")) {
            if (floor_objects < 9) {
                AddBeaconSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "cactus")) {
            if (cactus_objects < 4 && floor_objects < 10) {
                AddCactusSpec(specs, label, cactus_objects % 2 == 0 ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(index));
                ++cactus_objects;
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "rock") || ContainsSubstring(lower, "ridge") || ContainsSubstring(lower, "berm") ||
            ContainsSubstring(lower, "outcrop") || ContainsSubstring(lower, "boulder")) {
            if (rock_objects < 5 && floor_objects < 10) {
                AddRockSpec(specs, label, rock_objects % 2 == 0 ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(index));
                ++rock_objects;
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "gate") || ContainsSubstring(lower, "door") || ContainsSubstring(lower, "portal") ||
            ContainsSubstring(lower, "shutter") || ContainsSubstring(lower, "barrier") ||
            ContainsSubstring(lower, "fence") || ContainsSubstring(lower, "rail")) {
            if (gate_objects < 1) {
                AddGateSpec(specs, shell, label, static_cast<int>(index));
                ++gate_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "ai server") || ContainsSubstring(lower, "mainframe") ||
            ContainsSubstring(lower, "accelerator") || ContainsSubstring(lower, "gpu") ||
            ContainsSubstring(lower, "tensor") || ContainsSubstring(lower, "inference")) {
            if (ai_server_objects < 4 && floor_objects < 10) {
                AddAiServerSpec(specs, label, ai_server_objects % 2 == 0 ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(index));
                ++ai_server_objects;
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "rack") || ContainsSubstring(lower, "server") || ContainsSubstring(lower, "pod")) {
            if (rack_objects < 6 && floor_objects < 10) {
                AddRackSpec(specs, label, rack_objects % 2 == 0 ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(index));
                ++rack_objects;
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "console") || ContainsSubstring(lower, "desk") || ContainsSubstring(lower, "pedestal")) {
            if (floor_objects < 10) {
                AddConsoleSpec(specs, label, kLayoutZoneCenter, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "hatch")) {
            if (floor_objects < 10) {
                AddHatchSpec(specs, label, kLayoutZoneBack, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "cooling") || ContainsSubstring(lower, "vent") || ContainsSubstring(lower, "chiller") ||
            ContainsSubstring(lower, "hvac") || ContainsSubstring(lower, "duct") ||
            ContainsSubstring(lower, "conduit") || ContainsSubstring(lower, "pipe") || ContainsSubstring(lower, "trench")) {
            if (cooling_objects < 4 && floor_objects < 10) {
                AddCoolingSpec(specs, label, cooling_objects % 2 == 0 ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(index));
                ++cooling_objects;
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "cabinet") || ContainsSubstring(lower, "locker")) {
            if (floor_objects < 10) {
                AddLockerSpec(specs, label, kLayoutZoneRight, static_cast<int>(index));
                ++floor_objects;
            }
            continue;
        }

        if (ContainsSubstring(lower, "crate") || ContainsSubstring(lower, "box") || ContainsSubstring(lower, "lockbox") ||
            ContainsSubstring(lower, "case") || ContainsSubstring(lower, "spool") || ContainsSubstring(lower, "cache")) {
            if (crate_objects < 4 && floor_objects < 10) {
                const LayoutZone zone = crate_objects % 2 == 0 ? kLayoutZoneRight : kLayoutZoneLeft;
                AddCrateSpec(specs, label, zone, static_cast<int>(index));
                ++crate_objects;
                ++floor_objects;
            }
            continue;
        }
    }
}

static bool HasPrimitive(const std::vector<LayoutObjectSpec>& specs, ScenePrimitive primitive)
{
    for (size_t index = 0; index < specs.size(); ++index) {
        if (specs[index].primitive == primitive) {
            return true;
        }
    }
    return false;
}

static bool HasSpecNameContaining(const std::vector<LayoutObjectSpec>& specs, const char* pattern)
{
    if (!pattern) {
        return false;
    }

    const std::string lower_pattern = ToLowerAsciiCopy(pattern);
    for (size_t index = 0; index < specs.size(); ++index) {
        if (ToLowerAsciiCopy(specs[index].name).find(lower_pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static LayoutZone InferConstraintZone(const std::string& lower_text, LayoutZone fallback_zone)
{
    if (ContainsSubstring(lower_text, "left") || ContainsSubstring(lower_text, "west")) {
        return kLayoutZoneLeft;
    }
    if (ContainsSubstring(lower_text, "right") || ContainsSubstring(lower_text, "east")) {
        return kLayoutZoneRight;
    }
    if (ContainsSubstring(lower_text, "center") || ContainsSubstring(lower_text, "central") || ContainsSubstring(lower_text, "middle")) {
        return kLayoutZoneCenter;
    }
    if (ContainsSubstring(lower_text, "front") || ContainsSubstring(lower_text, "entry") || ContainsSubstring(lower_text, "near")) {
        return kLayoutZoneFront;
    }
    if (ContainsSubstring(lower_text, "rear") || ContainsSubstring(lower_text, "back") || ContainsSubstring(lower_text, "far") ||
        ContainsSubstring(lower_text, "horizon")) {
        return kLayoutZoneBack;
    }
    return fallback_zone;
}

static void AddConstraintDrivenSpecs(
    const SpatialState& spatial_state,
    const RoomShell& shell,
    std::vector<LayoutObjectSpec>* specs)
{
    if (!specs || spatial_state.scene_constraints.empty()) {
        return;
    }

    for (size_t index = 0; index < spatial_state.scene_constraints.size(); ++index) {
        const std::string label = spatial_state.scene_constraints[index];
        const std::string lower = ToLowerAsciiCopy(label);
        const LayoutZone zone = InferConstraintZone(lower, shell.exterior ? kLayoutZoneBack : kLayoutZoneCenter);

        if (ShouldSkipLabelForShell(shell, lower)) {
            continue;
        }

        if (ContainsSubstring(lower, "crystal scanner") || ContainsSubstring(lower, "crystal detector") ||
            ContainsSubstring(lower, "sample instrument") || ContainsSubstring(lower, "affinity detector")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalScanner)) {
                AddCrystalScannerSpec(specs, label, zone, static_cast<int>(180 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "drill") || ContainsSubstring(lower, "extraction rig") ||
            ContainsSubstring(lower, "boring rig") || ContainsSubstring(lower, "mining rig")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabExtractionRig)) {
                AddExtractionRigSpec(specs, label, zone, static_cast<int>(185 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "prospect shelter") || ContainsSubstring(lower, "prospecting station") ||
            ContainsSubstring(lower, "field shelter") || ContainsSubstring(lower, "survey shelter")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabProspectShelter)) {
                AddProspectShelterSpec(specs, label, zone, static_cast<int>(190 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "atmospheric processor") || ContainsSubstring(lower, "air processor") ||
            ContainsSubstring(lower, "oxygen service") || ContainsSubstring(lower, "electrolyser") ||
            ContainsSubstring(lower, "extraction pump")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabAtmosphericProcessor)) {
                AddAtmosphericProcessorSpec(specs, label, zone, static_cast<int>(195 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "quarry pylon") || ContainsSubstring(lower, "datum pylon") ||
            ContainsSubstring(lower, "industrial pylon")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon)) {
                AddQuarryPylonSpec(specs, label, zone, static_cast<int>(198 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "survey beacon") || ContainsSubstring(lower, "navigation mast") ||
            ContainsSubstring(lower, "range beacon")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon)) {
                AddSurveyBeaconSpec(specs, label, zone, static_cast<int>(199 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "crystal")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalCluster)) {
                AddCrystalClusterSpec(specs, label, zone, static_cast<int>(200 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "ai server") || ContainsSubstring(lower, "mainframe") ||
            ContainsSubstring(lower, "gpu") || ContainsSubstring(lower, "accelerator") ||
            ContainsSubstring(lower, "tensor") || ContainsSubstring(lower, "inference")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabAiServer)) {
                AddAiServerSpec(specs, label, zone, static_cast<int>(200 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "rack") || ContainsSubstring(lower, "server bank") ||
            ContainsSubstring(lower, "server wall") || ContainsSubstring(lower, "pod row")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabRack)) {
                AddRackSpec(specs, label, zone, static_cast<int>(220 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "cooling") || ContainsSubstring(lower, "vent") ||
            ContainsSubstring(lower, "chiller") || ContainsSubstring(lower, "hvac") ||
            ContainsSubstring(lower, "duct") || ContainsSubstring(lower, "conduit") ||
            ContainsSubstring(lower, "pipe") || ContainsSubstring(lower, "trench")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCoolingUnit)) {
                AddCoolingSpec(specs, label, zone, static_cast<int>(240 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "console") || ContainsSubstring(lower, "pedestal") || ContainsSubstring(lower, "desk")) {
            if (!HasSpecNameContaining(*specs, "console")) {
                AddConsoleSpec(specs, label, zone, static_cast<int>(260 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "hatch")) {
            if (!HasSpecNameContaining(*specs, "hatch")) {
                AddHatchSpec(specs, label, zone == kLayoutZoneCenter ? kLayoutZoneBack : zone, static_cast<int>(280 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "crate") || ContainsSubstring(lower, "lockbox") ||
            ContainsSubstring(lower, "case") || ContainsSubstring(lower, "spool") ||
            ContainsSubstring(lower, "cache")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
                AddCrateSpec(specs, label, zone, static_cast<int>(300 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "locker") || ContainsSubstring(lower, "cabinet")) {
            if (!HasSpecNameContaining(*specs, "locker") && !HasSpecNameContaining(*specs, "cabinet")) {
                AddLockerSpec(specs, label, zone == kLayoutZoneLeft ? kLayoutZoneLeft : kLayoutZoneRight, static_cast<int>(320 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "gate") || ContainsSubstring(lower, "door") || ContainsSubstring(lower, "portal") ||
            ContainsSubstring(lower, "barrier") || ContainsSubstring(lower, "fence") || ContainsSubstring(lower, "rail")) {
            if (!HasPrimitive(*specs, kScenePrimitivePrefabGate)) {
                AddGateSpec(specs, shell, label, static_cast<int>(340 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "beacon") || ContainsSubstring(lower, "lamp") || ContainsSubstring(lower, "light") ||
            ContainsSubstring(lower, "post") || ContainsSubstring(lower, "mast") || ContainsSubstring(lower, "pylon") ||
            ContainsSubstring(lower, "marker")) {
            if (!HasSpecNameContaining(*specs, "beacon") && !HasSpecNameContaining(*specs, "lamp") && !HasSpecNameContaining(*specs, "light")) {
                AddBeaconSpec(specs, label, zone, static_cast<int>(360 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "cactus")) {
            if (!HasSpecNameContaining(*specs, "cactus")) {
                AddCactusSpec(specs, label, zone, static_cast<int>(380 + index));
            }
            continue;
        }

        if (ContainsSubstring(lower, "rock") || ContainsSubstring(lower, "ridge") || ContainsSubstring(lower, "berm") ||
            ContainsSubstring(lower, "outcrop") || ContainsSubstring(lower, "boulder")) {
            if (!HasSpecNameContaining(*specs, "rock")) {
                AddRockSpec(specs, label, zone, static_cast<int>(400 + index));
            }
            continue;
        }
    }
}

static void AddArchetypeDefaults(
    const SpatialState& spatial_state,
    const RoomShell& shell,
    std::vector<LayoutObjectSpec>* specs)
{
    if (!specs) {
        return;
    }

    switch (shell.archetype) {
    case kLayoutArchetypeQuarryThreshold:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabGate)) {
            AddGateSpec(specs, shell, "survey pressure gate", 80);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon)) {
            AddSurveyBeaconSpec(specs, "datum beacon zero", kLayoutZoneBack, 81);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "prospecting cargo", kLayoutZoneRight, 82);
        }
        break;
    case kLayoutArchetypeExtractionField:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabExtractionRig)) {
            AddExtractionRigSpec(specs, "diamond extraction rig", kLayoutZoneCenter, 83);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon)) {
            AddQuarryPylonSpec(specs, "quarry datum pylon", kLayoutZoneBack, 84);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabAtmosphericProcessor)) {
            AddAtmosphericProcessorSpec(specs, "field oxygen processor", kLayoutZoneRight, 85);
        }
        break;
    case kLayoutArchetypeVenusPlateau:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon)) {
            AddSurveyBeaconSpec(specs, "near route beacon", kLayoutZoneLeft, 86);
            AddSurveyBeaconSpec(specs, "far route beacon", kLayoutZoneBack, 87);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon)) {
            AddQuarryPylonSpec(specs, "plateau datum", kLayoutZoneRight, 88);
        }
        break;
    case kLayoutArchetypeIndustrialServiceZone:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabAtmosphericProcessor)) {
            AddAtmosphericProcessorSpec(specs, "atmospheric processor", kLayoutZoneLeft, 89);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "service cargo", kLayoutZoneRight, 90);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabGate)) {
            AddGateSpec(specs, shell, "pressure threshold", 91);
        }
        break;
    case kLayoutArchetypeProspectingShelter:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabProspectShelter)) {
            AddProspectShelterSpec(specs, "field prospect shelter", kLayoutZoneBack, 91);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabAtmosphericProcessor)) {
            AddAtmosphericProcessorSpec(specs, "shelter oxygen service", kLayoutZoneLeft, 92);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "sample case", kLayoutZoneRight, 93);
        }
        if (!HasSpecNameContaining(*specs, "map")) {
            AddWallControlSpec(specs, "survey map display", kLayoutMountWallBack, kLayoutZoneBack, 94);
        }
        break;
    case kLayoutArchetypeScannerStation:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalScanner)) {
            AddCrystalScannerSpec(specs, "affinity scanner", kLayoutZoneCenter, 95);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalCluster)) {
            AddCrystalClusterSpec(specs, "reference crystal", kLayoutZoneRight, 96);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon)) {
            AddSurveyBeaconSpec(specs, "route comparison beacon", kLayoutZoneBack, 97);
        }
        break;
    case kLayoutArchetypeLabyrinthThreshold:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon)) {
            AddQuarryPylonSpec(specs, "west datum pylon", kLayoutZoneLeft, 98);
            AddQuarryPylonSpec(specs, "east datum pylon", kLayoutZoneRight, 99);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabSurveyBeacon)) {
            AddSurveyBeaconSpec(specs, "detour beacon", kLayoutZoneBack, 100);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalCluster)) {
            AddCrystalClusterSpec(specs, "suspended contact fragment", kLayoutZoneCenter, 101);
        }
        break;
    case kLayoutArchetypeQuarryCut:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrystalCluster)) {
            AddCrystalClusterSpec(specs, "exposed crystal vein", kLayoutZoneCenter, 101);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabExtractionRig)) {
            AddExtractionRigSpec(specs, "cut drill", kLayoutZoneLeft, 102);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabQuarryPylon)) {
            AddQuarryPylonSpec(specs, "vein marker", kLayoutZoneBack, 103);
        }
        break;
    case kLayoutArchetypeThresholdExterior:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabGate)) {
            AddGateSpec(specs, shell, "checkpoint gate", 100);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "service crate", kLayoutZoneRight, 101);
        }
        if (!HasSpecNameContaining(*specs, "reader")) {
            AddWallControlSpec(specs, "badge reader", kLayoutMountWallLeft, kLayoutZoneCenter, 102);
        }
        break;
    case kLayoutArchetypeRoofExterior:
        if (!HasSpecNameContaining(*specs, "hatch")) {
            AddHatchSpec(specs, "roof hatch", kLayoutZoneBack, 110);
        }
        if (!HasSpecNameContaining(*specs, "beacon")) {
            AddBeaconSpec(specs, "warning beacon", kLayoutZoneRight, 111);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "service crate", kLayoutZoneLeft, 112);
        }
        break;
    case kLayoutArchetypeYardExterior:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "yard crate", kLayoutZoneLeft, 120);
        }
        if (!HasSpecNameContaining(*specs, "beacon")) {
            AddBeaconSpec(specs, "warning beacon", kLayoutZoneRight, 121);
        }
        if (!HasSpecNameContaining(*specs, "hatch")) {
            AddHatchSpec(specs, "maintenance hatch", kLayoutZoneBack, 122);
        }
        break;
    case kLayoutArchetypeDesertExterior:
        if (!HasSpecNameContaining(*specs, "cactus")) {
            AddCactusSpec(specs, "cactus watch", kLayoutZoneLeft, 125);
            AddCactusSpec(specs, "cactus watch east", kLayoutZoneRight, 126);
        }
        if (!HasSpecNameContaining(*specs, "rock")) {
            AddRockSpec(specs, "rock outcrop west", kLayoutZoneLeft, 127);
            AddRockSpec(specs, "rock outcrop east", kLayoutZoneRight, 128);
            AddRockSpec(specs, "rock outcrop rear", kLayoutZoneBack, 129);
        }
        if (!HasSpecNameContaining(*specs, "marker")) {
            AddBeaconSpec(specs, "range marker", kLayoutZoneBack, 130);
        }
        break;
    case kLayoutArchetypeServerAisles:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabRack)) {
            AddRackSpec(specs, "rack block left", kLayoutZoneLeft, 130);
            AddRackSpec(specs, "rack block right", kLayoutZoneRight, 131);
            AddRackSpec(specs, "rack block left rear", kLayoutZoneLeft, 132);
            AddRackSpec(specs, "rack block right rear", kLayoutZoneRight, 133);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCoolingUnit)) {
            AddCoolingSpec(specs, "cooling block", kLayoutZoneLeft, 134);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "maintenance crate", kLayoutZoneCenter, 135);
        }
        break;
    case kLayoutArchetypeControlHub:
        if (!HasSpecNameContaining(*specs, "console")) {
            AddConsoleSpec(specs, "central console", kLayoutZoneCenter, 140);
        }
        if (!HasSpecNameContaining(*specs, "locker")) {
            AddLockerSpec(specs, "control cabinet", kLayoutZoneRight, 141);
        }
        AddWallControlSpec(specs, "status panel", kLayoutMountWallBack, kLayoutZoneBack, 142);
        break;
    case kLayoutArchetypeBackupVault:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabRack)) {
            AddRackSpec(specs, "vault rack left", kLayoutZoneLeft, 150);
            AddRackSpec(specs, "vault rack right", kLayoutZoneRight, 151);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "sealed pod", kLayoutZoneCenter, 152);
        }
        if (!HasSpecNameContaining(*specs, "hatch")) {
            AddHatchSpec(specs, "vault hatch", kLayoutZoneBack, 153);
        }
        break;
    case kLayoutArchetypeCoolingBay:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCoolingUnit)) {
            AddCoolingSpec(specs, "cooling block left", kLayoutZoneLeft, 160);
            AddCoolingSpec(specs, "cooling block right", kLayoutZoneRight, 161);
        }
        if (!HasSpecNameContaining(*specs, "console")) {
            AddConsoleSpec(specs, "service console", kLayoutZoneCenter, 162);
        }
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "drip tray", kLayoutZoneBack, 163);
        }
        break;
    case kLayoutArchetypeServiceInterior:
    default:
        if (!HasPrimitive(*specs, kScenePrimitivePrefabCrate)) {
            AddCrateSpec(specs, "maintenance crate", kLayoutZoneRight, 170);
        }
        if (!HasSpecNameContaining(*specs, "panel")) {
            AddWallControlSpec(specs, "service panel", kLayoutMountWallLeft, kLayoutZoneCenter, 171);
        }
        if (ListContainsSubstring(spatial_state.visible_objects, "rack") && !HasPrimitive(*specs, kScenePrimitivePrefabRack)) {
            AddRackSpec(specs, "service rack", kLayoutZoneLeft, 172);
        }
        break;
    }
}

static int CountEarlierZoneObjects(
    const std::vector<LayoutObjectSpec>& specs,
    size_t object_index,
    LayoutMount mount,
    LayoutZone zone)
{
    int count = 0;
    for (size_t index = 0; index < object_index; ++index) {
        if (specs[index].mount == mount && specs[index].zone == zone) {
            ++count;
        }
    }
    return count;
}

static void ClampFloorPosition(const RoomShell& shell, const LayoutObjectSpec& spec, Vec3* pos)
{
    if (!pos) {
        return;
    }

    const float x_margin = 0.45f;
    const float z_margin = 0.55f;
    const float min_x = -shell.half_width + spec.size.x * 0.5f + x_margin;
    const float max_x = shell.half_width - spec.size.x * 0.5f - x_margin;
    const float min_z = -shell.half_depth + shell.front_margin + spec.size.z * 0.5f;
    const float max_z = shell.half_depth - spec.size.z * 0.5f - z_margin;
    pos->x = Clamp(pos->x, min_x, max_x);
    pos->z = Clamp(pos->z, min_z, max_z);
    pos->y = spec.size.y * 0.5f;
}

static Vec3 ComputeFloorInitialPosition(
    const RoomShell& shell,
    const std::vector<LayoutObjectSpec>& specs,
    size_t object_index)
{
    const LayoutObjectSpec& spec = specs[object_index];
    const int zone_rank = CountEarlierZoneObjects(specs, object_index, spec.mount, spec.zone);
    Vec3 pos(0.0f, spec.size.y * 0.5f, 0.0f);

    switch (spec.zone) {
    case kLayoutZoneLeft:
        pos.x = -shell.half_width * 0.54f;
        break;
    case kLayoutZoneRight:
        pos.x = shell.half_width * 0.54f;
        break;
    case kLayoutZoneCenter:
        pos.x = (zone_rank % 2 == 0 ? -0.4f : 0.4f) * static_cast<float>(zone_rank / 2 + 1);
        break;
    case kLayoutZoneFront:
        pos.x = (zone_rank % 2 == 0 ? -1.1f : 1.1f);
        break;
    case kLayoutZoneBack:
    default:
        pos.x = (zone_rank % 2 == 0 ? -1.2f : 1.2f);
        break;
    }

    float stride = spec.size.z + 1.2f;
    if (stride < 2.0f) {
        stride = 2.0f;
    }
    if (stride > 3.2f) {
        stride = 3.2f;
    }

    switch (spec.zone) {
    case kLayoutZoneFront:
        pos.z = -shell.half_depth * 0.10f + zone_rank * 1.6f;
        break;
    case kLayoutZoneBack:
        pos.z = shell.half_depth * 0.50f - zone_rank * 1.7f;
        break;
    case kLayoutZoneCenter:
        pos.z = shell.half_depth * 0.08f + static_cast<float>(zone_rank - 1) * 1.7f;
        break;
    case kLayoutZoneLeft:
    case kLayoutZoneRight:
    default:
        pos.z = -shell.half_depth * 0.16f + static_cast<float>(zone_rank) * stride;
        break;
    }

    ClampFloorPosition(shell, spec, &pos);
    return pos;
}

static Vec3 ComputeMountedPosition(
    const RoomShell& shell,
    const std::vector<LayoutObjectSpec>& specs,
    size_t object_index)
{
    const LayoutObjectSpec& spec = specs[object_index];
    const int zone_rank = CountEarlierZoneObjects(specs, object_index, spec.mount, spec.zone);
    Vec3 pos(0.0f, 1.42f, 0.0f);

    if (spec.mount == kLayoutMountWallLeft || spec.mount == kLayoutMountWallRight) {
        pos.x = spec.mount == kLayoutMountWallLeft
            ? -shell.half_width + spec.size.x * 0.5f + 0.06f
            : shell.half_width - spec.size.x * 0.5f - 0.06f;
        pos.y = 1.38f + static_cast<float>(zone_rank % 2) * 0.34f;
        if (spec.zone == kLayoutZoneBack) {
            pos.z = shell.half_depth * 0.55f - static_cast<float>(zone_rank) * 1.8f;
        } else if (spec.zone == kLayoutZoneCenter) {
            pos.z = shell.half_depth * 0.12f + static_cast<float>(zone_rank - 1) * 1.9f;
        } else {
            pos.z = -shell.half_depth * 0.10f + static_cast<float>(zone_rank) * 1.8f;
        }
        return pos;
    }

    pos.z = shell.half_depth - spec.size.z * 0.5f - 0.08f;
    pos.y = 1.42f + static_cast<float>(zone_rank % 2) * 0.30f;
    if (spec.zone == kLayoutZoneBack) {
        pos.x = (zone_rank % 3 == 0) ? 0.0f : ((zone_rank % 2 == 0) ? -2.0f : 2.0f);
    } else {
        pos.x = (zone_rank % 2 == 0 ? -1.5f : 1.5f);
    }
    return pos;
}

static void SolveFloorLayout(const RoomShell& shell, std::vector<PlacedLayoutObject>* objects)
{
    if (!objects) {
        return;
    }

    for (int iteration = 0; iteration < 24; ++iteration) {
        bool moved = false;

        for (size_t index = 0; index < objects->size(); ++index) {
            PlacedLayoutObject& object = (*objects)[index];
            if (object.spec.mount != kLayoutMountFloor) {
                continue;
            }

            if (shell.corridor_half_width > 0.0f && object.spec.blocks_corridor) {
                const float half_width = object.spec.size.x * 0.5f;
                const float limit = shell.corridor_half_width + half_width + 0.20f;
                if (fabsf(object.pos.x) < limit) {
                    object.pos.x = object.pos.x < 0.0f ? -limit : limit;
                    if (fabsf(object.pos.x) < 0.05f) {
                        object.pos.x = (index % 2 == 0) ? -limit : limit;
                    }
                    ClampFloorPosition(shell, object.spec, &object.pos);
                    moved = true;
                }
            }
        }

        for (size_t i = 0; i < objects->size(); ++i) {
            PlacedLayoutObject& a = (*objects)[i];
            if (a.spec.mount != kLayoutMountFloor) {
                continue;
            }

            for (size_t j = i + 1; j < objects->size(); ++j) {
                PlacedLayoutObject& b = (*objects)[j];
                if (b.spec.mount != kLayoutMountFloor) {
                    continue;
                }

                const float overlap_x =
                    (a.spec.size.x * 0.5f + b.spec.size.x * 0.5f + 0.30f) - fabsf(a.pos.x - b.pos.x);
                const float overlap_z =
                    (a.spec.size.z * 0.5f + b.spec.size.z * 0.5f + 0.30f) - fabsf(a.pos.z - b.pos.z);

                if (overlap_x <= 0.0f || overlap_z <= 0.0f) {
                    continue;
                }

                if (overlap_x < overlap_z) {
                    const float push = overlap_x * 0.5f + 0.05f;
                    if (a.pos.x <= b.pos.x) {
                        a.pos.x -= push;
                        b.pos.x += push;
                    } else {
                        a.pos.x += push;
                        b.pos.x -= push;
                    }
                } else {
                    const float push = overlap_z * 0.5f + 0.05f;
                    if (a.pos.z <= b.pos.z) {
                        a.pos.z -= push;
                        b.pos.z += push;
                    } else {
                        a.pos.z += push;
                        b.pos.z -= push;
                    }
                }

                ClampFloorPosition(shell, a.spec, &a.pos);
                ClampFloorPosition(shell, b.spec, &b.pos);
                moved = true;
            }
        }

        if (!moved) {
            break;
        }
    }
}

static void AppendSkyLine(std::string* scene_text, const SpatialState& spatial_state)
{
    float zenith = 0.01f;
    float horizon = 0.24f;
    float nadir = 0.00f;
    float band = 0.34f;
    float curve = 1.95f;
    float noise = 0.12f;
    float star_density = 0.0030f;
    float star_intensity = 1.30f;
    float star_radius = 0.095f;

    if (spatial_state.time_of_day == kTimeDay) {
        zenith = 0.05f;
        horizon = 0.32f;
        nadir = 0.00f;
        band = 0.38f;
        curve = 1.85f;
        noise = 0.11f;
        star_density = 0.0f;
        star_intensity = 0.0f;
        star_radius = 0.0f;
    } else if (spatial_state.time_of_day == kTimeNight) {
        zenith = 0.00f;
        horizon = 0.18f;
        nadir = 0.00f;
        band = 0.30f;
        curve = 2.05f;
        noise = 0.14f;
        star_density = 0.0042f;
        star_intensity = 1.55f;
        star_radius = 0.10f;
    }

    if (spatial_state.visibility_level == kVisibilityLow) {
        horizon -= 0.03f;
        noise += 0.03f;
    } else if (spatial_state.visibility_level == kVisibilityDusty) {
        horizon += 0.02f;
        noise += 0.02f;
    }

    if (spatial_state.desert_state == kDesertDusty) {
        horizon += 0.02f;
        band += 0.03f;
        noise += 0.01f;
    }

    const std::string seed_text = spatial_state.room_title + "|" + spatial_state.room_summary + "|" + spatial_state.location_archetype;
    const uint32_t seed = HashTextSeed(seed_text);

    AppendLine(
        scene_text,
        "sky zenith(%.2f) horizon(%.2f) nadir(%.2f) band(%.2f) curve(%.2f) noise(%.2f) stars(%.4f,%.2f,%.3f) seed(%u)\n",
        zenith,
        horizon,
        nadir,
        band,
        curve,
        noise,
        star_density,
        star_intensity,
        star_radius,
        seed % 997u + 1u);
}

static void AppendExteriorShell(const RoomShell& shell, const SpatialState& spatial_state, std::string* scene_text)
{
    AppendLine(
        scene_text,
        "camera eye(0.0,%.2f,%.2f) target(0.0,%.2f,%.2f) up(0.0,1.0,0.0) fov(%.1f)\n",
        shell.camera_eye_y,
        shell.camera_eye_z,
        shell.camera_target_y,
        shell.camera_target_z,
        shell.camera_fov);
    AppendLine(
        scene_text,
        "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(%.1f) cone(12.0,28.0) intensity(%.1f)\n",
        shell.spotlight_range,
        shell.spotlight_intensity);
    AppendSkyLine(scene_text, spatial_state);
    AppendLine(
        scene_text,
        "plane \"ground\" pos(0.0,0.0,-1.5) normal(0.0,1.0,0.0) size(%.1f,%.1f) gray(%.2f)\n",
        shell.half_width * 11.0f,
        shell.half_depth * 11.0f,
        shell.floor_gray);

    if (shell.archetype == kLayoutArchetypeVenusPlateau ||
        shell.archetype == kLayoutArchetypeLabyrinthThreshold) {
        AppendLine(
            scene_text,
            "box \"horizon_mass_left\" pos(-11.0,0.45,16.0) size(18.0,0.90,5.0) gray(%.2f)\n",
            shell.floor_gray + 0.02f);
        AppendLine(
            scene_text,
            "box \"horizon_mass_right\" pos(12.0,0.35,18.0) size(19.0,0.70,4.2) gray(%.2f)\n",
            shell.floor_gray + 0.01f);
        return;
    } else if (shell.archetype == kLayoutArchetypeExtractionField) {
        AppendLine(scene_text, "box \"work_slab\" pos(0.0,0.18,3.0) size(14.0,0.36,19.0) gray(0.16)\n");
        AppendLine(scene_text, "box \"retaining_fin_left\" pos(-7.4,1.60,7.0) size(0.70,3.20,11.0) gray(0.20)\n");
        AppendLine(scene_text, "box \"retaining_fin_right\" pos(7.2,1.25,9.0) size(0.70,2.50,10.0) gray(0.19)\n");
        return;
    } else if (shell.archetype == kLayoutArchetypeQuarryCut) {
        AppendLine(scene_text, "box \"cut_wall_left\" pos(-6.2,2.20,5.0) size(4.4,4.40,25.0) gray(0.18)\n");
        AppendLine(scene_text, "box \"cut_wall_right\" pos(6.4,1.75,7.0) size(4.8,3.50,23.0) gray(0.19)\n");
        AppendLine(scene_text, "box \"cut_step_rear\" pos(0.0,0.55,14.0) size(8.0,1.10,4.0) gray(0.21)\n");
        return;
    } else if (shell.archetype == kLayoutArchetypeScannerStation) {
        AppendLine(scene_text, "box \"scanner_deck\" pos(0.0,0.22,3.0) size(15.0,0.44,19.0) gray(0.15)\n");
        AppendLine(scene_text, "box \"cantilever_left\" pos(-5.8,2.10,5.5) size(1.0,4.20,8.0) gray(0.20)\n");
        AppendLine(scene_text, "box \"cantilever_beam\" pos(-1.2,4.00,5.5) size(10.2,0.80,3.0) gray(0.22)\n");
        return;
    } else if (shell.archetype == kLayoutArchetypeQuarryThreshold) {
        AppendLine(scene_text, "box \"threshold_buttress_left\" pos(-6.8,2.10,8.0) size(2.2,4.20,5.0) gray(0.20)\n");
        AppendLine(scene_text, "box \"threshold_buttress_right\" pos(6.8,2.10,8.0) size(2.2,4.20,5.0) gray(0.20)\n");
        AppendLine(scene_text, "box \"split_crown_left\" pos(-3.7,4.10,8.0) size(4.6,1.0,5.0) gray(0.25)\n");
        AppendLine(scene_text, "box \"split_crown_right\" pos(3.7,4.10,8.0) size(4.6,1.0,5.0) gray(0.25)\n");
        return;
    }

    if (shell.archetype == kLayoutArchetypeDesertExterior) {
        AppendLine(
            scene_text,
            "box \"desert_ridge_left\" pos(%.2f,0.55,7.6) size(2.8,1.10,8.6) gray(%.2f)\n",
            -shell.half_width * 0.52f,
            shell.floor_gray + 0.01f);
        AppendLine(
            scene_text,
            "box \"desert_ridge_right\" pos(%.2f,0.50,8.4) size(3.2,1.00,8.2) gray(%.2f)\n",
            shell.half_width * 0.50f,
            shell.floor_gray + 0.02f);
        AppendLine(
            scene_text,
            "box \"desert_back_ridge\" pos(0.0,0.70,%.2f) size(%.1f,1.40,4.8) gray(%.2f)\n",
            shell.half_depth * 0.78f,
            shell.half_width * 1.45f,
            shell.floor_gray + 0.03f);
        return;
    } else if (shell.archetype == kLayoutArchetypeRoofExterior) {
        AppendLine(
            scene_text,
            "box \"roof_slab\" pos(0.0,0.28,1.0) size(%.1f,0.56,%.1f) gray(%.2f)\n",
            shell.half_width * 1.9f,
            shell.half_depth * 1.55f,
            0.15f);
        AppendLine(
            scene_text,
            "box \"parapet_left\" pos(%.2f,0.78,1.0) size(0.65,1.56,%.1f) gray(%.2f)\n",
            -shell.half_width * 0.82f,
            shell.half_depth * 1.52f,
            shell.trim_gray);
        AppendLine(
            scene_text,
            "box \"parapet_right\" pos(%.2f,0.78,1.0) size(0.65,1.56,%.1f) gray(%.2f)\n",
            shell.half_width * 0.82f,
            shell.half_depth * 1.52f,
            shell.trim_gray);
        AppendLine(
            scene_text,
            "box \"parapet_back\" pos(0.0,0.78,%.2f) size(%.1f,1.56,0.65) gray(%.2f)\n",
            shell.half_depth * 0.86f,
            shell.half_width * 1.64f,
            shell.trim_gray);
    } else {
        AppendLine(
            scene_text,
            "box \"compound_mass\" pos(0.0,%.2f,%.2f) size(%.1f,%.1f,3.8) gray(%.2f)\n",
            shell.height * 0.48f,
            shell.half_depth * 0.72f,
            shell.half_width * 1.2f,
            shell.height * 0.96f,
            shell.wall_gray);
        AppendLine(
            scene_text,
            "box \"compound_crown\" pos(0.0,%.2f,%.2f) size(%.1f,0.42,2.0) gray(%.2f)\n",
            shell.height * 0.86f,
            shell.half_depth * 0.72f,
            shell.half_width * 1.02f,
            shell.trim_gray);
        AppendLine(
            scene_text,
            "box \"wall_left\" pos(%.2f,1.35,1.2) size(0.75,2.70,%.1f) gray(%.2f)\n",
            -shell.half_width * 0.86f,
            shell.half_depth * 1.10f,
            shell.wall_gray);
        AppendLine(
            scene_text,
            "box \"wall_right\" pos(%.2f,1.35,1.2) size(0.75,2.70,%.1f) gray(%.2f)\n",
            shell.half_width * 0.86f,
            shell.half_depth * 1.10f,
            shell.wall_gray);
    }
}

static void AppendInteriorShell(const RoomShell& shell, std::string* scene_text)
{
    AppendLine(
        scene_text,
        "camera eye(0.0,%.2f,%.2f) target(0.0,%.2f,%.2f) up(0.0,1.0,0.0) fov(%.1f)\n",
        shell.camera_eye_y,
        shell.camera_eye_z,
        shell.camera_target_y,
        shell.camera_target_z,
        shell.camera_fov);
    AppendLine(
        scene_text,
        "spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(%.1f) cone(12.0,28.0) intensity(%.1f)\n",
        shell.spotlight_range,
        shell.spotlight_intensity);
    AppendLine(
        scene_text,
        "plane \"floor\" pos(0.0,0.0,0.0) normal(0.0,1.0,0.0) size(%.1f,%.1f) gray(%.2f)\n",
        shell.half_width * 2.0f,
        shell.half_depth * 2.0f,
        shell.floor_gray);
    AppendLine(
        scene_text,
        "plane \"ceiling\" pos(0.0,%.2f,0.0) normal(0.0,-1.0,0.0) size(%.1f,%.1f) gray(%.2f)\n",
        shell.height,
        shell.half_width * 2.0f,
        shell.half_depth * 2.0f,
        shell.ceiling_gray);
    AppendLine(
        scene_text,
        "box \"wall_back\" pos(0.0,%.2f,%.2f) size(%.1f,%.1f,0.60) gray(%.2f)\n",
        shell.height * 0.5f,
        shell.half_depth,
        shell.half_width * 2.0f,
        shell.height,
        shell.wall_gray);
    AppendLine(
        scene_text,
        "box \"wall_left\" pos(%.2f,%.2f,0.0) size(0.60,%.1f,%.1f) gray(%.2f)\n",
        -shell.half_width,
        shell.height * 0.5f,
        shell.height,
        shell.half_depth * 2.0f,
        shell.wall_gray);
    AppendLine(
        scene_text,
        "box \"wall_right\" pos(%.2f,%.2f,0.0) size(0.60,%.1f,%.1f) gray(%.2f)\n",
        shell.half_width,
        shell.height * 0.5f,
        shell.height,
        shell.half_depth * 2.0f,
        shell.wall_gray);

    if (shell.archetype == kLayoutArchetypeCoolingBay) {
        AppendLine(
            scene_text,
            "box \"ceiling_plenum\" pos(0.0,%.2f,%.2f) size(%.1f,0.55,%.1f) gray(%.2f)\n",
            shell.height - 0.28f,
            shell.half_depth * 0.38f,
            shell.half_width * 1.3f,
            shell.half_depth * 0.62f,
            shell.trim_gray);
    } else if (shell.archetype == kLayoutArchetypeServerAisles) {
        AppendLine(
            scene_text,
            "plane \"light_strip\" pos(0.0,%.2f,2.4) normal(0.0,-1.0,0.0) size(1.2,7.8) gray(0.45) emit(1.6)\n",
            shell.height - 0.01f);
    } else if (shell.archetype == kLayoutArchetypeControlHub || shell.archetype == kLayoutArchetypeBackupVault) {
        AppendLine(
            scene_text,
            "box \"threshold_frame\" pos(0.0,%.2f,%.2f) size(3.8,%.2f,0.42) gray(%.2f)\n",
            shell.height * 0.48f,
            shell.half_depth * 0.78f,
            shell.height * 0.96f,
            shell.trim_gray);
    }
}

static void AppendPlacedObject(std::string* scene_text, const PlacedLayoutObject& object)
{
    switch (object.spec.primitive) {
    case kScenePrimitivePrefabGate:
        AppendLine(
            scene_text,
            "prefab_gate \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.has_detail) {
            AppendLine(scene_text, " detail(%.2f)", object.spec.detail);
        }
        if (object.spec.bars > 0) {
            AppendLine(scene_text, " bars(%d)", object.spec.bars);
        }
        AppendLine(scene_text, "\n");
        break;
    case kScenePrimitivePrefabRack:
        AppendLine(
            scene_text,
            "prefab_rack \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.has_detail) {
            AppendLine(scene_text, " detail(%.2f)", object.spec.detail);
        }
        AppendLine(scene_text, "\n");
        break;
    case kScenePrimitivePrefabCrate:
        AppendLine(
            scene_text,
            "prefab_crate \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.has_detail) {
            AppendLine(scene_text, " detail(%.2f)", object.spec.detail);
        }
        AppendLine(scene_text, "\n");
        break;
    case kScenePrimitivePrefabCoolingUnit:
        AppendLine(
            scene_text,
            "prefab_cooling_unit \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.has_detail) {
            AppendLine(scene_text, " detail(%.2f)", object.spec.detail);
        }
        AppendLine(scene_text, "\n");
        break;
    case kScenePrimitivePrefabAiServer:
        AppendLine(
            scene_text,
            "prefab_ai_server \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.has_detail) {
            AppendLine(scene_text, " detail(%.2f)", object.spec.detail);
        }
        AppendLine(scene_text, "\n");
        break;
    case kScenePrimitivePrefabSurveyBeacon:
        AppendLine(
            scene_text,
            "prefab_survey_beacon \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabCrystalScanner:
        AppendLine(
            scene_text,
            "prefab_crystal_scanner \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f) glow(0.28)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabCrystalCluster:
        AppendLine(
            scene_text,
            "prefab_crystal_cluster \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) glow(0.34)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabExtractionRig:
        AppendLine(
            scene_text,
            "prefab_extraction_rig \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabProspectShelter:
        AppendLine(
            scene_text,
            "prefab_prospect_shelter \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabQuarryPylon:
        AppendLine(
            scene_text,
            "prefab_quarry_pylon \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabAtmosphericProcessor:
        AppendLine(
            scene_text,
            "prefab_atmospheric_processor \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f) detail(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray,
            object.spec.detail);
        break;
    case kScenePrimitivePrefabCactusSentinel:
        AppendLine(
            scene_text,
            "prefab_cactus_sentinel \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabCactusFork:
        AppendLine(
            scene_text,
            "prefab_cactus_fork \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabCactusCluster:
        AppendLine(
            scene_text,
            "prefab_cactus_cluster \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabRockLow:
        AppendLine(
            scene_text,
            "prefab_rock_low \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabRockWide:
        AppendLine(
            scene_text,
            "prefab_rock_wide \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabRockTall:
        AppendLine(
            scene_text,
            "prefab_rock_tall \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitivePrefabRockSpire:
        AppendLine(
            scene_text,
            "prefab_rock_spire \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)\n",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        break;
    case kScenePrimitiveBox:
    default:
        AppendLine(
            scene_text,
            "box \"%s\" pos(%.2f,%.2f,%.2f) size(%.2f,%.2f,%.2f) gray(%.2f)",
            object.spec.name.c_str(),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z,
            object.spec.gray);
        if (object.spec.emissive && object.spec.emit > 0.0f) {
            AppendLine(scene_text, " emit(%.2f)", object.spec.emit);
        }
        AppendLine(scene_text, "\n");
        break;
    }
}

static bool BuildProceduralSceneText(
    const SpatialState& spatial_state,
    std::string* scene_text,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene_text) {
        SetError(error_buffer, error_buffer_size, "Invalid scene text target: %s", "(null)");
        return false;
    }

    const RoomShell shell = BuildRoomShell(spatial_state);
    std::vector<LayoutObjectSpec> specs;
    BuildVisibleObjectSpecs(spatial_state, shell, &specs);
    AddConstraintDrivenSpecs(spatial_state, shell, &specs);
    AddArchetypeDefaults(spatial_state, shell, &specs);

    std::vector<PlacedLayoutObject> objects;
    objects.reserve(specs.size());
    for (size_t index = 0; index < specs.size(); ++index) {
        PlacedLayoutObject object;
        object.spec = specs[index];
        if (object.spec.mount == kLayoutMountFloor) {
            object.pos = ComputeFloorInitialPosition(shell, specs, index);
        } else {
            object.pos = ComputeMountedPosition(shell, specs, index);
        }
        objects.push_back(object);
    }

    SolveFloorLayout(shell, &objects);

    scene_text->clear();
    const std::string room_name = spatial_state.room_title.empty() ? "Generated Room" : spatial_state.room_title;
    AppendLine(scene_text, "room \"%s\"\n", room_name.c_str());
    if (shell.exterior) {
        AppendExteriorShell(shell, spatial_state, scene_text);
    } else {
        AppendInteriorShell(shell, scene_text);
    }

    for (size_t index = 0; index < objects.size(); ++index) {
        AppendPlacedObject(scene_text, objects[index]);
    }

    if (scene_text->empty()) {
        SetErrorFormat(error_buffer, error_buffer_size, "Scene compiler produced an empty scene for room \"%s\"", room_name.c_str());
        return false;
    }

    return true;
}

static bool BuildProceduralSceneDebugReport(
    const SpatialState& spatial_state,
    std::string* report_text,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!report_text) {
        SetError(error_buffer, error_buffer_size, "Invalid scene debug target: %s", "(null)");
        return false;
    }

    const RoomShell shell = BuildRoomShell(spatial_state);
    std::vector<LayoutObjectSpec> specs;
    BuildVisibleObjectSpecs(spatial_state, shell, &specs);
    AddConstraintDrivenSpecs(spatial_state, shell, &specs);
    AddArchetypeDefaults(spatial_state, shell, &specs);

    std::vector<PlacedLayoutObject> objects;
    objects.reserve(specs.size());
    for (size_t index = 0; index < specs.size(); ++index) {
        PlacedLayoutObject object;
        object.spec = specs[index];
        if (object.spec.mount == kLayoutMountFloor) {
            object.pos = ComputeFloorInitialPosition(shell, specs, index);
        } else {
            object.pos = ComputeMountedPosition(shell, specs, index);
        }
        objects.push_back(object);
    }
    SolveFloorLayout(shell, &objects);

    report_text->clear();
    AppendLine(report_text, "Scene compiler source: procedural\n");
    AppendLine(
        report_text,
        "Shell: archetype=%s exterior=%s size=(%.2f, %.2f, %.2f) corridor_half_width=%.2f front_margin=%.2f camera_fov=%.2f\n",
        LayoutArchetypeToString(shell.archetype),
        shell.exterior ? "yes" : "no",
        shell.half_width,
        shell.half_depth,
        shell.height,
        shell.corridor_half_width,
        shell.front_margin,
        shell.camera_fov);
    AppendLine(
        report_text,
        "Materials: floor=%.2f ceiling=%.2f wall=%.2f trim=%.2f spotlight(range=%.2f intensity=%.2f)\n",
        shell.floor_gray,
        shell.ceiling_gray,
        shell.wall_gray,
        shell.trim_gray,
        shell.spotlight_range,
        shell.spotlight_intensity);

    AppendLine(report_text, "Spatial inputs:\n");
    AppendLine(report_text, "  room_title=%s\n", spatial_state.room_title.empty() ? "(empty)" : spatial_state.room_title.c_str());
    AppendLine(report_text, "  location_archetype=%s\n", spatial_state.location_archetype.empty() ? "(empty)" : spatial_state.location_archetype.c_str());
    AppendLine(report_text, "  world_band=%s\n", DescribeSpatialWorldBand(spatial_state));
    AppendLine(report_text, "  survey_base_relation=%s\n", DescribeSpatialGateRelation(spatial_state));
    AppendLine(report_text, "  sky_exposure=%s\n", DescribeSpatialSkyExposure(spatial_state));

    AppendLine(report_text, "Resolved specs: %zu\n", specs.size());
    for (size_t index = 0; index < specs.size(); ++index) {
        const LayoutObjectSpec& spec = specs[index];
        AppendLine(
            report_text,
            "  [%02zu] name=%s primitive=%s mount=%s zone=%s size=(%.2f, %.2f, %.2f) gray=%.2f detail=%s%.2f emissive=%s emit=%.2f bars=%d blocks_corridor=%s\n",
            index,
            spec.name.c_str(),
            ScenePrimitiveToString(spec.primitive),
            LayoutMountToString(spec.mount),
            LayoutZoneToString(spec.zone),
            spec.size.x,
            spec.size.y,
            spec.size.z,
            spec.gray,
            spec.has_detail ? "" : "(none) ",
            spec.has_detail ? spec.detail : 0.0f,
            spec.emissive ? "yes" : "no",
            spec.emit,
            spec.bars,
            spec.blocks_corridor ? "yes" : "no");
    }

    AppendLine(report_text, "Placed objects: %zu\n", objects.size());
    for (size_t index = 0; index < objects.size(); ++index) {
        const PlacedLayoutObject& object = objects[index];
        AppendLine(
            report_text,
            "  [%02zu] name=%s primitive=%s mount=%s zone=%s pos=(%.2f, %.2f, %.2f) size=(%.2f, %.2f, %.2f)\n",
            index,
            object.spec.name.c_str(),
            ScenePrimitiveToString(object.spec.primitive),
            LayoutMountToString(object.spec.mount),
            LayoutZoneToString(object.spec.zone),
            object.pos.x,
            object.pos.y,
            object.pos.z,
            object.spec.size.x,
            object.spec.size.y,
            object.spec.size.z);
    }

    return true;
}

}  // namespace

bool BuildCanonicalSpatialState(LocationId location_id, SpatialState* spatial_state)
{
    if (!spatial_state) {
        return false;
    }

    *spatial_state = SpatialState();
    spatial_state->location_id = location_id;

    switch (location_id) {
    case kLocationQuarryThreshold:
        SetCommonQuarryThresholdSpatialState(spatial_state);
        return true;
    case kLocationExtractionField:
        SetCommonExtractionFieldSpatialState(spatial_state);
        return true;
    case kLocationCrystalCut:
        SetCommonCrystalCutSpatialState(spatial_state);
        return true;
    case kLocationScannerStation:
        SetCommonScannerStationSpatialState(spatial_state);
        return true;
    case kLocationSurveyPlateau:
        SetCommonSurveyPlateauSpatialState(spatial_state);
        return true;
    case kLocationLabyrinthThreshold:
        SetCommonLabyrinthThresholdSpatialState(spatial_state);
        return true;
    case kLocationProspectShelter:
        SetCommonProspectShelterSpatialState(spatial_state);
        return true;
    case kLocationGate:
        SetCommonGateSpatialState(spatial_state);
        return true;
    case kLocationServerAisles:
        SetCommonAislesSpatialState(spatial_state);
        return true;
    case kLocationRoofWatch:
        SetCommonRoofSpatialState(spatial_state);
        return true;
    default:
        spatial_state->location_archetype = "unknown";
        return false;
    }
}

bool BuildSceneTextFromSpatialState(
    const SpatialState& spatial_state,
    std::string* scene_text,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene_text) {
        SetError(error_buffer, error_buffer_size, "Invalid scene text target: %s", "(null)");
        return false;
    }

    const char* fixture_path = spatial_state.canonical_fixture.empty()
        ? CanonicalFixturePath(spatial_state.location_id)
        : spatial_state.canonical_fixture.c_str();
    if (fixture_path && fixture_path[0]) {
        return ReadTextFile(fixture_path, scene_text, error_buffer, error_buffer_size);
    }

    return BuildProceduralSceneText(spatial_state, scene_text, error_buffer, error_buffer_size);
}

bool BuildSceneDebugReportFromSpatialState(
    const SpatialState& spatial_state,
    std::string* report_text,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!report_text) {
        SetError(error_buffer, error_buffer_size, "Invalid scene debug target: %s", "(null)");
        return false;
    }

    const char* fixture_path = spatial_state.canonical_fixture.empty()
        ? CanonicalFixturePath(spatial_state.location_id)
        : spatial_state.canonical_fixture.c_str();
    if (fixture_path && fixture_path[0]) {
        report_text->clear();
        AppendLine(report_text, "Scene compiler source: canonical fixture\n");
        AppendLine(report_text, "Fixture path: %s\n", fixture_path);
        AppendLine(report_text, "Room title: %s\n", spatial_state.room_title.empty() ? "(empty)" : spatial_state.room_title.c_str());
        AppendLine(report_text, "Location archetype: %s\n", spatial_state.location_archetype.empty() ? "(empty)" : spatial_state.location_archetype.c_str());
        AppendLine(report_text, "World band: %s\n", DescribeSpatialWorldBand(spatial_state));
        AppendLine(report_text, "Survey base relation: %s\n", DescribeSpatialGateRelation(spatial_state));
        AppendLine(report_text, "Sky exposure: %s\n", DescribeSpatialSkyExposure(spatial_state));
        return true;
    }

    return BuildProceduralSceneDebugReport(spatial_state, report_text, error_buffer, error_buffer_size);
}

bool CompileSpatialStateToScene(const SpatialState& spatial_state, Scene* scene, char* error_buffer, size_t error_buffer_size)
{
    if (!scene) {
        SetError(error_buffer, error_buffer_size, "Invalid compile target: %s", "(null)");
        return false;
    }

    std::string scene_text;
    if (!BuildSceneTextFromSpatialState(spatial_state, &scene_text, error_buffer, error_buffer_size)) {
        return false;
    }

    const char* fixture_path = spatial_state.canonical_fixture.empty()
        ? CanonicalFixturePath(spatial_state.location_id)
        : spatial_state.canonical_fixture.c_str();
    const char* scene_name = fixture_path && fixture_path[0]
        ? fixture_path
        : (spatial_state.room_title.empty() ? "(generated.scene)" : spatial_state.room_title.c_str());
    return LoadSceneFromSceneText(scene_name, scene_text.c_str(), scene, error_buffer, error_buffer_size);
}

bool AuditSceneCandidateText(
    const char* scene_name,
    const char* scene_text,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    return LoadSceneFromSceneText(scene_name ? scene_name : "(candidate.scene)", scene_text, scene, error_buffer, error_buffer_size);
}

}  // namespace liminal
