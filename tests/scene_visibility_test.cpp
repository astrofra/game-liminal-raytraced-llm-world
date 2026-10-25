// DUO-019: geometric visibility recipe for AuditDynamicScene.
//
// These expectations describe the NEW validator (fixed 256x128 visibility grid,
// first OPAQUE impact, glass traversal, one object ID per geometry directive and
// projected/occluded landmark accounting). They intentionally do not encode the
// behaviour of the legacy validator still present in this worktree.
//
// Scope: AuditDynamicScene only, with design=nullptr so that planned-design rules
// (focal coverage, support, passages, bounds) cannot mask the visibility recipe.
// No timing and no LLM call.

#include "scene_design.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;

#define CHECK(condition) do { \
    if(!(condition)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": "+#condition); \
} while(false)

namespace {

constexpr int kGridWidth = 256;
constexpr int kGridHeight = 128;
constexpr std::size_t kGridPixels =
    static_cast<std::size_t>(kGridWidth) * static_cast<std::size_t>(kGridHeight);

// Canonical local-scene header (assets/scene_generation/local_v1.json).
const char* const kHeader =
R"SCENE(room "DUO-019 visibility"
camera eye(0,1.7,-10) target(0,2,8) up(0,1,0) fov(55)
sky zenith(0.12) horizon(0.35) nadir(0.02) band(0.3) curve(1.5) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(2.5) color(1,0.92,0.72) angular_radius(0)
spotlight panel(1,1) offset(0,0,0.35) range(40) cone(20,60) intensity(100)
plane "desert_floor" pos(0,0,10) normal(0,1,0) size(100,100) gray(0.3)
)SCENE";

// Thin landmark centred in the fixed frame: about 3 x 26 grid samples at z=28.
const char* const kLandmark = "box \"marker\" pos(0,3,28) size(1,8,1) gray(0.8)\n";
// Low foreground volume: hides the lower half of the landmark only.
const char* const kLowObstacle = "box \"obstacle\" pos(0,1,25) size(8,2,2) gray(0.5)\n";
const char* const kOpaqueWall = "box \"wall\" pos(0,3,20) size(40,20,1) gray(0.4)\n";
const char* const kGlassWall = "glass_box \"wall\" pos(0,3,20) size(40,20,1) gray(0.2)\n";
// Multi-material prefab away from the landmark sightline.
const char* const kGatePrefab = "prefab_gate \"arch\" pos(-10,2,8) size(3,4,1) gray(0.6) detail(0.28) bars(1)\n";

std::string Program(const std::string& extra) { return std::string(kHeader) + extra; }

SceneViewConstraints LandmarkView(const char* landmark) {
    SceneViewConstraints view;
    view.landmark = landmark;
    view.aspect = 2.0f;
    return view;
}

bool HasDiagnostic(const SceneAuditReport& report, const char* code) {
    for (const auto& diagnostic : report.diagnostics)
        if (diagnostic.code == code) return true;
    return false;
}

void PrintReport(const SceneAuditReport& report) {
    std::printf("  accepted=%d projected=%d occluded=%d ids=%zu depths=%zu names=%zu\n",
        report.accepted ? 1 : 0, report.landmark_projected_pixels, report.landmark_occluded_pixels,
        report.object_ids.size(), report.depth.size(), report.object_names.size());
    for (const auto& diagnostic : report.diagnostics)
        std::printf("  diagnostic %s %s: %s\n", diagnostic.code.c_str(),
            diagnostic.object_id.c_str(), diagnostic.detail.c_str());
}

void ExpectAccepted(const char* label, const SceneAuditReport& report) {
    if (!report.accepted) {
        std::printf("%s must be accepted\n", label);
        PrintReport(report);
    }
    CHECK(report.accepted);
}

void ExpectRejected(const char* label, const SceneAuditReport& report) {
    if (report.accepted) std::printf("%s must be rejected\n", label);
    CHECK(!report.accepted);
}

SceneAuditReport Audit(const std::string& program, const SceneViewConstraints* view) {
    SceneDesignContext context;
    Scene scene;
    SceneAuditReport report;
    const bool accepted = AuditDynamicScene(program, context, nullptr, &scene, &report, view);
    if (accepted != report.accepted) {
        std::printf("return value/audit.accepted mismatch\n");
        PrintReport(report);
    }
    CHECK(accepted == report.accepted);
    return report;
}

int CountNamedPixels(const SceneAuditReport& report, const std::string& name) {
    int count = 0;
    for (const auto id : report.object_ids)
        if (id != 0 && static_cast<std::size_t>(id) < report.object_names.size() &&
            report.object_names[id] == name) ++count;
    return count;
}

// The recipe requires an infinite background depth; a large sentinel is tolerated.
bool IsBackgroundDepth(float depth) { return !std::isfinite(depth) || depth > 1.0e29f; }

bool MinNamedDepth(const SceneAuditReport& report, const std::string& name, float* minimum) {
    if (report.depth.size() != report.object_ids.size()) return false;
    bool found = false;
    for (std::size_t index = 0; index < report.object_ids.size(); ++index) {
        const auto id = report.object_ids[index];
        if (id == 0 || static_cast<std::size_t>(id) >= report.object_names.size()) continue;
        if (report.object_names[id] != name) continue;
        if (!found || report.depth[index] < *minimum) *minimum = report.depth[index];
        found = true;
    }
    return found;
}

void ThinLandmarkAboveLowObstacleIsAccepted() {
    const SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport report = Audit(Program(std::string(kLandmark) + kLowObstacle), &view);
    ExpectAccepted("thin landmark above a low obstacle", report);
    CHECK(report.object_ids.size() == kGridPixels);
    CHECK(report.depth.size() == kGridPixels);
    CHECK(report.object_names.size() >= 2);
    CHECK(report.landmark_projected_pixels >= 8);
    CHECK(report.landmark_projected_pixels <= static_cast<int>(kGridPixels));
    CHECK(report.landmark_occluded_pixels >= 0);
    CHECK(report.landmark_occluded_pixels <= report.landmark_projected_pixels);
    CHECK(static_cast<long long>(report.landmark_occluded_pixels) * 100 <=
          static_cast<long long>(report.landmark_projected_pixels) * 95);
    // The floor and the low obstacle mask part of the landmark, not all of it.
    CHECK(report.landmark_occluded_pixels > 0);
    const int visible = CountNamedPixels(report, "marker");
    CHECK(visible > 0);
    CHECK(visible <= report.landmark_projected_pixels);
    CHECK(!HasDiagnostic(report, "landmark_not_visible"));
}

void ThinLandmarkEscapingTheGridIsAccepted() {
    const SceneViewConstraints view = LandmarkView("marker");
    const std::string program =
        Program("box \"marker\" pos(0,3,28) size(0.02,0.02,0.02) gray(0.8)\n");
    const SceneAuditReport report = Audit(program, &view);
    // A landmark thinner than one grid sample must not be rejected for lack of samples.
    ExpectAccepted("landmark thinner than one grid sample", report);
    CHECK(report.object_ids.size() == kGridPixels);
    CHECK(report.depth.size() == kGridPixels);
    CHECK(report.landmark_projected_pixels < 8);
    CHECK(report.landmark_occluded_pixels >= 0);
    CHECK(report.landmark_occluded_pixels <= report.landmark_projected_pixels);
    CHECK(CountNamedPixels(report, "marker") <= report.landmark_projected_pixels);
    CHECK(!HasDiagnostic(report, "landmark_not_visible"));
}

void OpaqueWallHidingTheLandmarkIsRejected() {
    const SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport report = Audit(Program(std::string(kLandmark) + kOpaqueWall), &view);
    ExpectRejected("landmark hidden by an opaque wall", report);
    CHECK(HasDiagnostic(report, "landmark_not_visible"));
    CHECK(report.landmark_projected_pixels >= 8);
    CHECK(report.landmark_occluded_pixels <= report.landmark_projected_pixels);
    CHECK(static_cast<long long>(report.landmark_occluded_pixels) * 100 >
          static_cast<long long>(report.landmark_projected_pixels) * 95);
    CHECK(CountNamedPixels(report, "marker") == 0);
}

void GlassWallDoesNotHideTheLandmark() {
    const SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport report = Audit(Program(std::string(kLandmark) + kGlassWall), &view);
    ExpectAccepted("landmark behind a glass wall", report);
    CHECK(!HasDiagnostic(report, "landmark_not_visible"));
    CHECK(report.landmark_projected_pixels >= 8);
    CHECK(report.landmark_occluded_pixels <= report.landmark_projected_pixels);
    CHECK(static_cast<long long>(report.landmark_occluded_pixels) * 100 <=
          static_cast<long long>(report.landmark_projected_pixels) * 95);
    // Visibility resolves the first OPAQUE impact, so glass never owns a sample.
    CHECK(CountNamedPixels(report, "marker") > 0);
    CHECK(CountNamedPixels(report, "wall") == 0);
}

void CameraInsideSolidsIsRejected() {
    const SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport opaque = Audit(Program(std::string(kLandmark) +
        "box \"camera_shell\" pos(0,1.7,-10) size(4,4,4) gray(0.4)\n"), &view);
    ExpectRejected("camera inside an opaque box", opaque);
    CHECK(HasDiagnostic(opaque, "camera_inside_mass"));
    // Glass is traversed for visibility but remains solid for the camera.
    const SceneAuditReport glass = Audit(Program(std::string(kLandmark) +
        "glass_box \"camera_glass\" pos(0,1.7,-10) size(4,4,4) gray(0.2)\n"), &view);
    ExpectRejected("camera inside a glass box", glass);
}

void BlockedReturnIsRejectedWhenRequired() {
    const std::string program = Program(std::string(kLandmark) + kLowObstacle +
        "box \"return_crate\" pos(0.6,0.5,-9.5) size(0.5,0.8,0.5) gray(0.6)\n");
    SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport open = Audit(program, &view);
    ExpectAccepted("return corridor clear without the clearance requirement", open);
    view.require_return_clearance = true;
    const SceneAuditReport guarded = Audit(program, &view);
    ExpectRejected("return corridor blocked with the clearance requirement", guarded);
    CHECK(HasDiagnostic(guarded, "return_clearance_blocked"));
}

void ReportGridGroupingAndDepthAreConsistent() {
    const SceneViewConstraints view = LandmarkView("marker");
    const SceneAuditReport report =
        Audit(Program(std::string(kLandmark) + kLowObstacle + kGatePrefab), &view);
    ExpectAccepted("grid report fixture", report);
    CHECK(report.object_ids.size() == kGridPixels);
    CHECK(report.depth.size() == kGridPixels);
    CHECK(report.object_names.size() >= 2);

    std::set<unsigned int> used_ids;
    std::size_t background_pixels = 0;
    for (std::size_t index = 0; index < report.object_ids.size(); ++index) {
        const auto id = report.object_ids[index];
        CHECK(static_cast<std::size_t>(id) < report.object_names.size());
        CHECK(report.depth[index] > 0.0f);
        if (id == 0) {
            ++background_pixels;
            CHECK(IsBackgroundDepth(report.depth[index]));
        } else {
            CHECK(!IsBackgroundDepth(report.depth[index]));
            used_ids.insert(id);
        }
    }
    CHECK(background_pixels > 0);
    // Background plus one ID per geometry directive: floor, marker, obstacle, prefab gate.
    CHECK(used_ids.size() == 4);

    CHECK(CountNamedPixels(report, "desert_floor") > 0);
    CHECK(CountNamedPixels(report, "marker") > 0);
    CHECK(CountNamedPixels(report, "obstacle") > 0);

    // The prefab gate owns several materials and child boxes but a single object ID.
    int prefab_ids = 0;
    for (const auto id : used_ids) {
        const std::string& name = report.object_names[id];
        CHECK(!name.empty());
        if (name != "desert_floor" && name != "marker" && name != "obstacle") ++prefab_ids;
    }
    CHECK(prefab_ids == 1);

    float marker_min_depth = 0.0f;
    float obstacle_min_depth = 0.0f;
    CHECK(MinNamedDepth(report, "marker", &marker_min_depth));
    CHECK(MinNamedDepth(report, "obstacle", &obstacle_min_depth));
    CHECK(obstacle_min_depth > 0.0f);
    CHECK(obstacle_min_depth < marker_min_depth);
    CHECK(marker_min_depth > 30.0f);
    CHECK(marker_min_depth < 45.0f);
}

int Run(const char* name, void (*test)()) {
    try {
        test();
        std::printf("[PASS] %s\n", name);
        return 0;
    } catch (const std::exception& error) {
        std::printf("[FAIL] %s: %s\n", name, error.what());
        return 1;
    }
}

} // namespace

int main() {
    int failures = 0;
    failures += Run("thin_landmark_above_low_obstacle_is_accepted", ThinLandmarkAboveLowObstacleIsAccepted);
    failures += Run("thin_landmark_escaping_grid_is_accepted", ThinLandmarkEscapingTheGridIsAccepted);
    failures += Run("opaque_wall_hiding_landmark_is_rejected", OpaqueWallHidingTheLandmarkIsRejected);
    failures += Run("glass_wall_does_not_hide_landmark", GlassWallDoesNotHideTheLandmark);
    failures += Run("camera_inside_solids_is_rejected", CameraInsideSolidsIsRejected);
    failures += Run("blocked_return_is_rejected_when_required", BlockedReturnIsRejectedWhenRequired);
    failures += Run("report_grid_grouping_and_depth_are_consistent", ReportGridGroupingAndDepthAreConsistent);
    if (failures != 0) {
        std::printf("%d case(s) failed\n", failures);
        return 1;
    }
    std::printf("all 7 visibility cases passed\n");
    return 0;
}
