#include "known_barrier.h"
#include "investigation.h"
#include "turn_runner.h"
#include "scene.h"
#include "scene_design.h"
#include "scene_hash.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
using namespace liminal;
using nlohmann::json;
#define KB(c) do { if (!(c)) throw std::runtime_error(std::string("Known barrier ") + std::to_string(__LINE__) + ": " + #c); } while (false)
namespace {
const char* kRoom = "generated:room_0001";
const char* kOtherRoom = "generated:room_0002";
const char* kPrefix = "known_obstruction_";
struct Built { std::string composed; Scene scene; };
std::string FixtureSceneText() {
    return LocalSceneHeader() + "box \"floor\" pos(0,-0.1,0) size(12,0.2,12) gray(0.45)\n"
        "box \"backdrop\" pos(0,2,10) size(14,4,0.4) gray(0.55)\n";
}

SessionState MakeFixture(const std::string& forward, const std::string& return_direction) {
    SessionState state; char error[1024] = {};
    KB(InitializeSessionState(kLocationQuarryThreshold, &state, error, sizeof(error)));
    json design = json::object();
    if (!forward.empty()) design["forward_direction"] = forward;
    if (!return_direction.empty()) design["return_direction"] = return_direction;
    GeneratedRoom room;
    room.room_id = kRoom; room.scene_contract_version = 1;
    room.scene_text = FixtureSceneText(); room.spatial_state = state.spatial_state;
    room.scene_design_json = design.dump();
    room.scene_hash = SceneTextHash(room.scene_text);
    room.scene_plan_hash = SceneTextHash(room.scene_design_json);
    state.generated_rooms.push_back(room);
    state.current_place_id = kRoom; state.hard_state.current_location_id = kLocationUnknown;
    state.spatial_state = state.generated_rooms.back().spatial_state;
    PreserveLegacyInvestigationPlaces(&state);
    return state;
}
void AddBarrier(SessionState* state, const std::string& place, CardinalDirection direction, bool discovered = true) {
    InvisibleBarrier barrier;
    barrier.place_id = place; barrier.direction = direction;
    barrier.evidence = "Known barrier fixture"; barrier.discovered = discovered;
    state->invisible_barriers.push_back(barrier);
}
Scene LoadBase(const std::string& text) {
    Scene scene; char error[1024] = {};
    KB(LoadSceneFromSceneText("known barrier fixture", text.c_str(), &scene, error, sizeof(error)));
    return scene;
}
bool Build(const std::string& base, const SessionState& state, Built* out) {
    std::string error;
    return BuildKnownBarrierScene(base, state, &out->composed, &out->scene, &error);
}

bool HasPrefix(const std::string& id) { return id.rfind(kPrefix, 0) == 0; }
int GlassWalls(const Scene& scene) {
    int count = 0; for (const auto& box : scene.boxes) if (box.glass && HasPrefix(box.id)) ++count;
    return count;
}
std::vector<std::string> ObstructionIds(const Scene& scene) {
    std::vector<std::string> ids;
    for (const auto& box : scene.boxes) if (HasPrefix(box.id)) ids.push_back(box.id);
    std::sort(ids.begin(), ids.end()); return ids;
}
std::vector<std::string> BoxIds(const Scene& scene) {
    std::vector<std::string> ids; for (const auto& box : scene.boxes) ids.push_back(box.id); return ids;
}
int Occurrences(const std::string& text, const std::string& needle) {
    int count = 0; size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) { ++count; pos += needle.size(); }
    return count;
}
const SceneBox* FindBox(const Scene& scene, const std::string& id) {
    for (const auto& box : scene.boxes) if (box.id == id) return &box; return nullptr;
}
bool SameCamera(const Camera& a, const Camera& b) { return std::memcmp(&a, &b, sizeof(Camera)) == 0; }
bool SameBox(const SceneBox& a, const SceneBox& b) {
    return a.id == b.id && a.glass == b.glass &&
        std::memcmp(a.corners.data(), b.corners.data(), sizeof(a.corners)) == 0;
}
// The authored boxes keep their identity; only new glass walls are appended.
void VerifyAuthoredBoxes(const Scene& base, const Scene& built) {
    for (const auto& origin : base.boxes) {
        const SceneBox* kept = FindBox(built, origin.id);
        KB(kept != nullptr && SameBox(origin, *kept));
    }
    KB(SameCamera(base.camera, built.camera));
}
BarrierViewSide SideFor(const std::string& forward, const std::string& return_direction, CardinalDirection direction) {
    SessionState state = MakeFixture(forward, return_direction); AddBarrier(&state, kRoom, direction);
    KB(HasKnownBarrier(state, direction)); return KnownBarrierSide(state, direction);
}

void HiddenUntilDiscoveredAndPresent() {
    const std::string base = FixtureSceneText(); const Scene parsed = LoadBase(base);
    KB(parsed.boxes.size() == 2);
    SessionState state = MakeFixture("north", ""); KB(!HasKnownBarrier(state, kDirectionNorth));
    Built built; KB(Build(base, state, &built));
    KB(built.composed == base && GlassWalls(built.scene) == 0);
    VerifyAuthoredBoxes(parsed, built.scene);
    KB(built.scene.boxes.size() == parsed.boxes.size());
    AddBarrier(&state, kRoom, kDirectionNorth, false); KB(!HasKnownBarrier(state, kDirectionNorth));
    KB(Build(base, state, &built) && built.composed == base && GlassWalls(built.scene) == 0);
    AddBarrier(&state, kOtherRoom, kDirectionEast, true); KB(!HasKnownBarrier(state, kDirectionEast));
    KB(Build(base, state, &built) && built.composed == base && GlassWalls(built.scene) == 0);
    AddBarrier(&state, kRoom, kDirectionUnknown, true);
    KB(!HasKnownBarrier(state, kDirectionUnknown));
    KB(!HasKnownBarrier(state, static_cast<CardinalDirection>(42)));
    KB(Build(base, state, &built) && built.composed == base);
}

void FaceCameraOrientation() {
    KB(SideFor("north", "", kDirectionNorth) == BarrierViewSide::front);
    KB(SideFor("north", "", kDirectionEast) == BarrierViewSide::right);
    KB(SideFor("north", "", kDirectionSouth) == BarrierViewSide::back);
    KB(SideFor("north", "", kDirectionWest) == BarrierViewSide::left);
    KB(SideFor("east", "", kDirectionEast) == BarrierViewSide::front);
    KB(SideFor("east", "", kDirectionSouth) == BarrierViewSide::right);
    KB(SideFor("east", "", kDirectionWest) == BarrierViewSide::back);
    KB(SideFor("east", "", kDirectionNorth) == BarrierViewSide::left);
    // Returning from the south means the camera faces north.
    KB(SideFor("", "south", kDirectionNorth) == BarrierViewSide::front);
    KB(SideFor("", "south", kDirectionEast) == BarrierViewSide::right);
    KB(SideFor("", "west", kDirectionEast) == BarrierViewSide::front);
    KB(SideFor("", "west", kDirectionSouth) == BarrierViewSide::right);
    // The canonical threshold faces east.
    SessionState threshold = MakeFixture("north", "");
    threshold.current_place_id = threshold.origin_place_id;
    threshold.hard_state.current_location_id = kLocationQuarryThreshold;
    threshold.generated_rooms.clear();
    AddBarrier(&threshold, threshold.current_place_id, kDirectionEast);
    AddBarrier(&threshold, threshold.current_place_id, kDirectionNorth);
    KB(HasKnownBarrier(threshold, kDirectionEast));
    KB(KnownBarrierSide(threshold, kDirectionEast) == BarrierViewSide::front);
    KB(KnownBarrierSide(threshold, kDirectionNorth) == BarrierViewSide::left);
    // Movement history never reorients the room.
    SessionState returned = MakeFixture("north", ""); AddBarrier(&returned, kRoom, kDirectionNorth);
    for (int i = 0; i < 3; ++i) {
        SessionTurnRecord record;
        record.origin_place_id = kRoom; record.place_id = kRoom;
        record.engine_outcome = "Moved south"; returned.history.push_back(record);
    }
    returned.hard_state.move_count = 7;
    KB(KnownBarrierSide(returned, kDirectionNorth) == BarrierViewSide::front);
    // Without reliable provenance no side is invented.
    SessionState blind = MakeFixture("", ""); AddBarrier(&blind, kRoom, kDirectionNorth);
    KB(KnownBarrierSide(blind, kDirectionNorth) == BarrierViewSide::unknown);
    Built built; KB(Build(FixtureSceneText(), blind, &built));
    KB(built.composed == FixtureSceneText() && GlassWalls(built.scene) == 0);
}

void VisibleSidesOnly() {
    const std::string base = FixtureSceneText(); const Scene parsed = LoadBase(base); int covered = 0;
    for (CardinalDirection direction : {kDirectionNorth, kDirectionEast, kDirectionWest}) {
        SessionState state = MakeFixture("north", ""); AddBarrier(&state, kRoom, direction);
        Built built; KB(Build(base, state, &built));
        KB(GlassWalls(built.scene) == 1 && ObstructionIds(built.scene).size() == 1);
        KB(Occurrences(built.composed, kPrefix) == 1);
        VerifyAuthoredBoxes(parsed, built.scene);
        KB(built.scene.boxes.size() == parsed.boxes.size() + 1); ++covered;
    }
    KB(covered == 3);
    SessionState behind = MakeFixture("north", ""); AddBarrier(&behind, kRoom, kDirectionSouth);
    KB(KnownBarrierSide(behind, kDirectionSouth) == BarrierViewSide::back);
    Built built; KB(Build(base, behind, &built));
    KB(built.composed == base && GlassWalls(built.scene) == 0);
    VerifyAuthoredBoxes(parsed, built.scene);
}

void RepeatedComposition() {
    const std::string base = FixtureSceneText(); SessionState state = MakeFixture("north", "");
    AddBarrier(&state, kRoom, kDirectionNorth); AddBarrier(&state, kRoom, kDirectionEast);
    AddBarrier(&state, kRoom, kDirectionWest);
    std::string before; KB(SerializeSessionStateToJsonString(state, &before));
    const std::string investigation = SerializeInvestigation(state.investigation);
    Built first, again, layered;
    KB(Build(base, state, &first) && Build(base, state, &again));
    KB(first.composed == again.composed && BoxIds(first.scene) == BoxIds(again.scene));
    KB(GlassWalls(first.scene) == 3);
    const std::vector<std::string> ids = ObstructionIds(first.scene);
    KB(ids.size() == 3 && std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    KB(Occurrences(first.composed, kPrefix) == 3);
    // A previously composed program is replaced, never doubled.
    KB(Build(first.composed, state, &layered));
    KB(GlassWalls(layered.scene) == 3 && ObstructionIds(layered.scene) == ids);
    KB(layered.composed == first.composed);
    std::string after; KB(SerializeSessionStateToJsonString(state, &after));
    KB(before == after && SerializeInvestigation(state.investigation) == investigation);
    KB(state.generated_rooms.front().scene_text == base);
    KB(state.spatial_state.blocked_exits == state.generated_rooms.front().spatial_state.blocked_exits);
}

void SerializedRoundTripAndLeaving() {
    const std::string base = FixtureSceneText(); SessionState state = MakeFixture("north", "");
    AddBarrier(&state, kRoom, kDirectionNorth); AddBarrier(&state, kRoom, kDirectionEast, false);
    std::string text; KB(SerializeSessionStateToJsonString(state, &text));
    SessionState restored; char error[1024] = {};
    KB(ParseSessionStateFromJson(text.c_str(), &restored, error, sizeof(error)));
    KB(restored.current_place_id == kRoom && HasKnownBarrier(restored, kDirectionNorth));
    KB(!HasKnownBarrier(restored, kDirectionEast));
    KB(KnownBarrierSide(restored, kDirectionNorth) == KnownBarrierSide(state, kDirectionNorth));
    Built original, roundtripped;
    KB(Build(base, state, &original) && Build(base, restored, &roundtripped));
    KB(original.composed == roundtripped.composed && BoxIds(original.scene) == BoxIds(roundtripped.scene));
    KB(GlassWalls(roundtripped.scene) == 1);
    SessionState elsewhere = restored; elsewhere.current_place_id = elsewhere.origin_place_id;
    Built away; KB(Build(base, elsewhere, &away));
    KB(away.composed == base && GlassWalls(away.scene) == 0);
    KB(!HasKnownBarrier(elsewhere, kDirectionNorth));
    SessionState returned = elsewhere; returned.current_place_id = kRoom;
    Built back; KB(Build(base, returned, &back));
    KB(GlassWalls(back.scene) == 1);
}
}  // namespace

void RunKnownBarrierTests() {
    HiddenUntilDiscoveredAndPresent();
    FaceCameraOrientation();
    VisibleSidesOnly();
    RepeatedComposition();
    SerializedRoundTripAndLeaving();
}
