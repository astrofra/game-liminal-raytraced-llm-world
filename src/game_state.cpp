#include "game_state.h"

#include <string.h>

namespace liminal {

namespace {

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

}  // namespace liminal
