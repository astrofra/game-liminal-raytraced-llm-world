// DUO-033: expectations for the coplanar-face cleanup pass, on the interface
// fixed by Codex in coplanar_cleanup.h.
//
// Scope: camera-facing coplanar overlap detection, minimal bounded
// translations, opposed-joint diagnostics, fixed ids, validation callback,
// trial/distance caps, analysis-only calls, preexisting glass exclusion and
// self-pair exclusion. Positions are compared through Scene::boxes after
// LoadSceneFromSceneText, so the persistent comment added to corrected output
// is not a geometry change. No timing, no LLM call and no file access: every
// scene is inline.
//
// The fixtures under tests/data/coplanar/ are synthetic scenes for the
// before/after rendering; they are not reconstructed user captures.

#include "coplanar_cleanup.h"

#include "scene.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;

#define CHECK(condition) do { \
    if(!(condition)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": "+#condition); \
} while(false)

namespace {

// Minimal valid scene: room, reference camera, sky, lights and a large ground
// plane. The camera looks at (0,1.7,5) from (0,1.7,-10); every fixture stays
// clear of its eye and local approaches.
const char* const kHeader =
R"SCENE(room "DUO-033 coplanar cleanup"
camera eye(0,1.7,-10) target(0,1.7,5) up(0,1,0) fov(50)
spotlight panel(1,1) offset(0,0,0.35) range(44) cone(15,36) intensity(58)
distant_light direction(0.75,-0.42,0.35) intensity(3.6) color(1.0,0.90,0.74) angular_radius(0.40)
sky zenith(0.04) horizon(0.35) nadir(0.00) band(0.34) curve(1.92) noise(0.12)
plane "desert_floor" pos(0,0,10) normal(0,1,0) size(120,160) gray(0.2)
)SCENE";

constexpr float kPositionTolerance = 1.0e-5f;
constexpr float kMovementTolerance = 1.0e-4f;
constexpr float kMinimalMove = 0.01f;
constexpr float kMaxCumulative = 0.05f;
constexpr std::size_t kMaxTranslations = 16;

// Case 1 baseline: two opaque boxes whose front faces share the plane z=4.5
// with a 2 m x 2 m common area. Their centres differ only along X.
const char* const kFrontalPair =
    "box \"a\" pos(-0.5,1,5) size(3,2,1) gray(0.25)\n"
    "box \"b\" pos(0.5,1,5) size(3,2,1) gray(0.75)\n";

std::string Program(const std::string& extra) { return std::string(kHeader) + extra; }

CoplanarCleanupResult Cleanup(const std::string& source,
                             const CoplanarCleanupOptions& options = CoplanarCleanupOptions())
{
    return CleanupCoplanarFaces(source, options);
}

void ExpectSuccess(const CoplanarCleanupResult& result)
{
    if (!result.success)
        throw std::runtime_error(std::string("CleanupCoplanarFaces failed: ") + result.error);
}

Scene LoadValidatedScene(const std::string& text, const char* name)
{
    Scene scene;
    char error_buffer[512] = {0};
    if (!LoadSceneFromSceneText(name, text.c_str(), &scene, error_buffer, sizeof(error_buffer))) {
        throw std::runtime_error(std::string("LoadSceneFromSceneText failed for ") + name +
            ": " + error_buffer);
    }
    return scene;
}

const SceneBox& FindBox(const Scene& scene, const char* id)
{
    for (const auto& box : scene.boxes)
        if (box.id == id) return box;
    throw std::runtime_error(std::string("missing box: ") + id);
}

bool Near(float value, float expected, float tolerance = 1.0e-4f)
{
    return std::fabs(value - expected) <= tolerance;
}

bool SameVec(const Vec3& a, const Vec3& b, float tolerance)
{
    return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

void ExpectBoxUnchanged(const Scene& before, const Scene& after, const char* id)
{
    const SceneBox& original = FindBox(before, id);
    const SceneBox& current = FindBox(after, id);
    CHECK(current.id == original.id);
    CHECK(SameVec(current.center, original.center, kPositionTolerance));
    CHECK(SameVec(current.size, original.size, kPositionTolerance));
    for (int axis = 0; axis < 3; ++axis)
        CHECK(SameVec(current.axes[axis], original.axes[axis], kPositionTolerance));
}

// A translation must keep size, orientation and id: only the centre may move.
void ExpectWholeBox(const Scene& before, const Scene& after, const char* id)
{
    const SceneBox& original = FindBox(before, id);
    const SceneBox& current = FindBox(after, id);
    CHECK(current.id == id);
    CHECK(SameVec(current.size, original.size, kPositionTolerance));
    for (int axis = 0; axis < 3; ++axis)
        CHECK(SameVec(current.axes[axis], original.axes[axis], kPositionTolerance));
}

float Displacement(const Scene& before, const Scene& after, const char* id)
{
    return Length(FindBox(after, id).center - FindBox(before, id).center);
}

bool HasKey(const nlohmann::json& object, const char* key)
{
    return object.is_object() && object.find(key) != object.end();
}

bool IsIdentifier(const nlohmann::json& value)
{
    return value.is_string() || value.is_number_integer();
}

std::string IdentifierText(const nlohmann::json& value)
{
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    throw std::runtime_error("identifier is neither a string nor an integer");
}

void ValidatePairSchema(const nlohmann::json& pair)
{
    CHECK(pair.is_object());
    CHECK(HasKey(pair, "block_a"));
    CHECK(IsIdentifier(pair.at("block_a")));
    CHECK(HasKey(pair, "block_b"));
    CHECK(IsIdentifier(pair.at("block_b")));
    CHECK(HasKey(pair, "face_a"));
    CHECK(IsIdentifier(pair.at("face_a")) || pair.at("face_a").is_number());
    CHECK(HasKey(pair, "face_b"));
    CHECK(IsIdentifier(pair.at("face_b")) || pair.at("face_b").is_number());
    CHECK(HasKey(pair, "classification"));
    CHECK(pair.at("classification").is_string());
    const std::string classification = pair.at("classification").get<std::string>();
    CHECK(classification == "same_direction_conflict" || classification == "opposed_joint");
    CHECK(HasKey(pair, "overlap_area"));
    CHECK(pair.at("overlap_area").is_number());
    CHECK(pair.at("overlap_area").get<double>() >= 0.0);
}

nlohmann::json ParseReport(const CoplanarCleanupResult& result)
{
    ExpectSuccess(result);
    nlohmann::json report;
    try {
        report = nlohmann::json::parse(result.report_json);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("invalid report_json: ") + error.what());
    }
    CHECK(report.is_object());
    CHECK(HasKey(report, "initial_pairs"));
    CHECK(report.at("initial_pairs").is_array());
    CHECK(HasKey(report, "final_pairs"));
    CHECK(report.at("final_pairs").is_array());
    CHECK(HasKey(report, "trials"));
    CHECK(report.at("trials").is_array());
    CHECK(HasKey(report, "moves"));
    CHECK(report.at("moves").is_array());
    CHECK(HasKey(report, "status"));
    CHECK(report.at("status").is_string());
    CHECK(!report.at("status").get<std::string>().empty());
    for (const auto& pair : report.at("initial_pairs")) ValidatePairSchema(pair);
    for (const auto& pair : report.at("final_pairs")) ValidatePairSchema(pair);
    return report;
}

std::size_t CountClassification(const nlohmann::json& pairs, const char* classification)
{
    std::size_t count = 0;
    for (const auto& pair : pairs)
        if (pair.at("classification").get<std::string>() == classification) ++count;
    return count;
}

bool PairLinks(const nlohmann::json& pair, const char* first, const char* second)
{
    const std::string a = IdentifierText(pair.at("block_a"));
    const std::string b = IdentifierText(pair.at("block_b"));
    return (a == first && b == second) || (a == second && b == first);
}

bool HasTargetedPairBetween(const nlohmann::json& pairs, const char* first, const char* second)
{
    for (const auto& pair : pairs) {
        if (pair.at("classification").get<std::string>() != "same_direction_conflict") continue;
        if (PairLinks(pair, first, second)) return true;
    }
    return false;
}

bool HasOpposedJointBetween(const nlohmann::json& pairs, const char* first, const char* second)
{
    for (const auto& pair : pairs) {
        if (pair.at("classification").get<std::string>() != "opposed_joint") continue;
        if (PairLinks(pair, first, second)) return true;
    }
    return false;
}

void FrontalCoplanarPairGetsMinimalTranslation()
{
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations >= std::size_t(1));
    CHECK(HasTargetedPairBetween(report.at("initial_pairs"), "a", "b"));

    const Scene before = LoadValidatedScene(source, "duo033_frontal_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_frontal_after");
    // Stable order keeps the first block and moves the second one.
    ExpectBoxUnchanged(before, after, "a");
    ExpectWholeBox(before, after, "b");

    const Vec3 shift = FindBox(after, "b").center - FindBox(before, "b").center;
    CHECK(Near(shift.x, 0.0f, 1.0e-6f));
    CHECK(Near(shift.y, 0.0f, 1.0e-6f));
    CHECK(Near(std::fabs(shift.z), kMinimalMove, kMovementTolerance));
    CHECK(Near(Length(shift), kMinimalMove, kMovementTolerance));

    // Reapplying the pass to its own output reuses the marker and moves nothing.
    const CoplanarCleanupResult reapplied = Cleanup(result.scene_text);
    ParseReport(reapplied);
    CHECK(reapplied.scene_text == result.scene_text);
    CHECK(reapplied.accepted_translations == std::size_t(0));
}

void SeparatedFrontalPlanesAreLeftAlone()
{
    // 0.02 m apart: well beyond the 1 mm plane tolerance.
    const std::string source = Program(
        "box \"a\" pos(-0.5,1,5) size(3,2,1) gray(0.25)\n"
        "box \"b\" pos(0.5,1,5.02) size(3,2,1) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));

    const Scene before = LoadValidatedScene(source, "duo033_separated_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_separated_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void SharedEdgeIsNotACoplanarConflict()
{
    // Front faces share the plane z=4.5 but meet only on the line x=0: zero area.
    const std::string source = Program(
        "box \"a\" pos(-1,1,5) size(2,2,1) gray(0.25)\n"
        "box \"b\" pos(1,1,5) size(2,2,1) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(!HasTargetedPairBetween(report.at("initial_pairs"), "a", "b"));

    const Scene before = LoadValidatedScene(source, "duo033_edge_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_edge_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void DuplicatedBoxesStayBounded()
{
    // Coincident duplicates: several coplanar faces qualify. The pass may
    // resolve or report a residual, but never beyond its translation and
    // distance caps.
    const std::string source = Program(
        "box \"a\" pos(0,1,5) size(3,2,1) gray(0.25)\n"
        "box \"b\" pos(0,1,5) size(3,2,1) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.accepted_translations <= kMaxTranslations);
    CHECK(report.at("moves").size() <= kMaxTranslations);
    if (result.remaining_conflicts > std::size_t(0))
        CHECK(!report.at("final_pairs").empty());

    const Scene before = LoadValidatedScene(source, "duo033_duplicate_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_duplicate_after");
    for (const char* id : {"a", "b"}) {
        ExpectWholeBox(before, after, id);
        // The public result exposes final positions; their net offset must stay
        // within the cumulative budget, and the accepted-move count is capped.
        CHECK(Displacement(before, after, id) <= kMaxCumulative + kMovementTolerance);
    }
}

void AdjacentBoxesReportAnOpposedJoint()
{
    // Stacked along Z: the touching faces are opposed and constructively joined.
    const std::string source = Program(
        "box \"a\" pos(0,1,5) size(3,2,1) gray(0.25)\n"
        "box \"b\" pos(0,1,6) size(3,2,1) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(HasOpposedJointBetween(report.at("initial_pairs"), "a", "b"));

    const Scene before = LoadValidatedScene(source, "duo033_adjacent_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_adjacent_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void ObliqueCoplanarFacesAreDetectedAndResolved()
{
    // Both boxes are turned Y=35 and offset by 0.7 m along their local X, so
    // their front faces share an oblique world plane with a positive overlap.
    const float yaw = 35.0f * kPi / 180.0f;
    char second_box[256] = {0};
    std::snprintf(second_box, sizeof(second_box),
        "box \"b\" pos(%.6f,1,%.6f) size(3,2,1) rot(0,35,0) gray(0.75)\n",
        std::cos(yaw) * 0.7f, 5.0f - std::sin(yaw) * 0.7f);
    const std::string source = Program(
        std::string("box \"a\" pos(0,1,5) size(3,2,1) rot(0,35,0) gray(0.25)\n") + second_box);

    CoplanarCleanupOptions analysis;
    analysis.apply_translations = false;
    const CoplanarCleanupResult detected = Cleanup(source, analysis);
    const nlohmann::json detected_report = ParseReport(detected);
    CHECK(detected.initial_conflicts > std::size_t(0));
    CHECK(detected.accepted_translations == std::size_t(0));
    CHECK(detected.scene_text == source);
    CHECK(HasTargetedPairBetween(detected_report.at("initial_pairs"), "a", "b"));

    bool positive_overlap = false;
    for (const auto& pair : detected_report.at("initial_pairs")) {
        if (!PairLinks(pair, "a", "b")) continue;
        if (pair.at("classification").get<std::string>() != "same_direction_conflict") continue;
        if (pair.at("overlap_area").get<double>() > 0.0) positive_overlap = true;
    }
    CHECK(positive_overlap);

    const CoplanarCleanupResult corrected = Cleanup(source);
    ParseReport(corrected);
    CHECK(corrected.initial_conflicts > std::size_t(0));
    CHECK(corrected.remaining_conflicts == std::size_t(0));
    CHECK(corrected.accepted_translations >= std::size_t(1));

    const Scene before = LoadValidatedScene(source, "duo033_oblique_before");
    const Scene after = LoadValidatedScene(corrected.scene_text, "duo033_oblique_after");
    for (const char* id : {"a", "b"}) {
        ExpectWholeBox(before, after, id);
        CHECK(Displacement(before, after, id) <= kMaxCumulative + kMovementTolerance);
    }
}

void YawedFrontFacesAreNotCoplanar()
{
    // Same centre, Y=0 and Y=30: the front faces meet at 30 degrees. The
    // horizontal planes y=0 and y=2 are dorsal for this camera and excluded.
    const std::string source = Program(
        "box \"a\" pos(0,1,5) size(3,2,1) rot(0,0,0) gray(0.25)\n"
        "box \"b\" pos(0,1,5) size(3,2,1) rot(0,30,0) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(CountClassification(report.at("initial_pairs"), "same_direction_conflict") == std::size_t(0));
    CHECK(!HasTargetedPairBetween(report.at("initial_pairs"), "a", "b"));

    const Scene before = LoadValidatedScene(source, "duo033_yawed_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_yawed_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void ThreeOverlappingBoxesResolveDeterministically()
{
    const std::string source = Program(
        "box \"a\" pos(-0.6,1,5) size(3,2,1) gray(0.2)\n"
        "box \"b\" pos(0,1,5) size(3,2,1) gray(0.5)\n"
        "box \"c\" pos(0.6,1,5) size(3,2,1) gray(0.8)\n");
    const CoplanarCleanupResult first = Cleanup(source);
    ParseReport(first);
    CHECK(first.initial_conflicts > std::size_t(0));
    CHECK(first.remaining_conflicts == std::size_t(0));
    CHECK(first.accepted_translations <= kMaxTranslations);

    const Scene before = LoadValidatedScene(source, "duo033_three_before");
    const Scene after = LoadValidatedScene(first.scene_text, "duo033_three_after");
    for (const char* id : {"a", "b", "c"}) {
        ExpectWholeBox(before, after, id);
        CHECK(Displacement(before, after, id) <= kMaxCumulative + kMovementTolerance);
    }

    const CoplanarCleanupResult second = Cleanup(source);
    ParseReport(second);
    CHECK(second.scene_text == first.scene_text);
    CHECK(second.initial_conflicts == first.initial_conflicts);
    CHECK(second.remaining_conflicts == first.remaining_conflicts);
    CHECK(second.accepted_translations == first.accepted_translations);

    const CoplanarCleanupResult reapplied = Cleanup(first.scene_text);
    ParseReport(reapplied);
    CHECK(reapplied.scene_text == first.scene_text);
    CHECK(reapplied.accepted_translations == std::size_t(0));
    CHECK(reapplied.remaining_conflicts == std::size_t(0));
}

void FixedIdsPreserveSceneAndResidual()
{
    CoplanarCleanupOptions options;
    options.fixed_ids = std::vector<std::string>{"a", "b"};
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source, options);
    ParseReport(result);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.remaining_conflicts > std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));

    const Scene before = LoadValidatedScene(source, "duo033_fixed_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_fixed_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");

    // A second execution on the marked output is byte-identical and moves
    // nothing further.
    const CoplanarCleanupResult reapplied = Cleanup(result.scene_text, options);
    ParseReport(reapplied);
    CHECK(reapplied.scene_text == result.scene_text);
    CHECK(reapplied.accepted_translations == std::size_t(0));
    CHECK(reapplied.remaining_conflicts > std::size_t(0));
}

void RejectingValidationCallbackKeepsGeometry()
{
    CoplanarCleanupOptions options;
    options.validate = [](const std::string&, std::string* error) {
        if (error) *error = "duo033_reject_all_trials";
        return false;
    };
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source, options);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.remaining_conflicts > std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(!report.at("trials").empty());
    CHECK(report.at("moves").empty());
    CHECK(report.dump().find("duo033_reject_all_trials") != std::string::npos);

    const Scene before = LoadValidatedScene(source, "duo033_rejected_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_rejected_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void SingleTrialBudgetDoesNotDrift()
{
    CoplanarCleanupOptions options;
    options.max_candidate_trials = 1;
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source, options);
    const nlohmann::json report = ParseReport(result);
    CHECK(report.at("trials").size() <= std::size_t(1));
    CHECK(result.accepted_translations <= std::size_t(1));

    const Scene before = LoadValidatedScene(source, "duo033_single_trial_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_single_trial_after");
    for (const char* id : {"a", "b"}) {
        ExpectWholeBox(before, after, id);
        // At most one 1 cm candidate can be accepted, so no drift is possible.
        CHECK(Displacement(before, after, id) <= kMinimalMove + kMovementTolerance);
    }
}

void SmallCumulativeBudgetRejectsOneCentimeter()
{
    CoplanarCleanupOptions options;
    options.max_cumulative_distance = 0.005;
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source, options);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.remaining_conflicts > std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(report.at("moves").empty());

    const Scene before = LoadValidatedScene(source, "duo033_small_budget_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_small_budget_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void AnalysisOnlyLeavesInputUntouched()
{
    CoplanarCleanupOptions options;
    options.apply_translations = false;
    const std::string source = Program(kFrontalPair);
    const CoplanarCleanupResult result = Cleanup(source, options);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.source_scene_text == source);
    CHECK(result.scene_text == source);
    CHECK(result.initial_conflicts > std::size_t(0));
    CHECK(result.remaining_conflicts == result.initial_conflicts);
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(!report.at("initial_pairs").empty());
    CHECK(report.at("final_pairs").size() == report.at("initial_pairs").size());
    CHECK(report.at("trials").empty());
    CHECK(report.at("moves").empty());

    const Scene before = LoadValidatedScene(source, "duo033_analysis_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_analysis_after");
    ExpectBoxUnchanged(before, after, "a");
    ExpectBoxUnchanged(before, after, "b");
}

void PreexistingGlassIsExcluded()
{
    // Same geometry as case 1, but the second volume is glass: the opaque pass
    // must not report a targeted overlap between them.
    const std::string source = Program(
        "box \"a\" pos(-0.5,1,5) size(3,2,1) gray(0.25)\n"
        "glass_box \"b\" pos(0.5,1,5) size(3,2,1) gray(0.75)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    CHECK(!HasTargetedPairBetween(report.at("initial_pairs"), "a", "b"));

    const Scene before = LoadValidatedScene(source, "duo033_glass_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_glass_after");
    ExpectBoxUnchanged(before, after, "a");
}

void SingleBoxHasNoSelfConflict()
{
    // One box only: its own six faces and triangles must never form a pair.
    const std::string source =
        Program("box \"solo\" pos(-0.5,1,5) size(3,2,1) gray(0.25)\n");
    const CoplanarCleanupResult result = Cleanup(source);
    const nlohmann::json report = ParseReport(result);
    CHECK(result.initial_conflicts == std::size_t(0));
    CHECK(result.remaining_conflicts == std::size_t(0));
    CHECK(result.accepted_translations == std::size_t(0));
    for (const auto& pair : report.at("initial_pairs"))
        CHECK(IdentifierText(pair.at("block_a")) != IdentifierText(pair.at("block_b")));

    const Scene before = LoadValidatedScene(source, "duo033_self_before");
    const Scene after = LoadValidatedScene(result.scene_text, "duo033_self_after");
    ExpectBoxUnchanged(before, after, "solo");
}

int RunTest(const char* name, void (*test)())
{
    try {
        test();
        std::printf("[PASS] %s\n", name);
        return 0;
    } catch (const std::exception& error) {
        std::printf("[FAIL] %s: %s\n", name, error.what());
        return 1;
    }
}

}  // namespace

int main()
{
    int failures = 0;
    failures += RunTest("frontal_coplanar_pair_gets_minimal_translation",
        FrontalCoplanarPairGetsMinimalTranslation);
    failures += RunTest("separated_frontal_planes_are_left_alone",
        SeparatedFrontalPlanesAreLeftAlone);
    failures += RunTest("shared_edge_is_not_a_coplanar_conflict", SharedEdgeIsNotACoplanarConflict);
    failures += RunTest("duplicated_boxes_stay_bounded", DuplicatedBoxesStayBounded);
    failures += RunTest("adjacent_boxes_report_an_opposed_joint", AdjacentBoxesReportAnOpposedJoint);
    failures += RunTest("oblique_coplanar_faces_are_detected_and_resolved",
        ObliqueCoplanarFacesAreDetectedAndResolved);
    failures += RunTest("yawed_front_faces_are_not_coplanar", YawedFrontFacesAreNotCoplanar);
    failures += RunTest("three_overlapping_boxes_resolve_deterministically",
        ThreeOverlappingBoxesResolveDeterministically);
    failures += RunTest("fixed_ids_preserve_scene_and_residual", FixedIdsPreserveSceneAndResidual);
    failures += RunTest("rejecting_validation_callback_keeps_geometry",
        RejectingValidationCallbackKeepsGeometry);
    failures += RunTest("single_trial_budget_does_not_drift", SingleTrialBudgetDoesNotDrift);
    failures += RunTest("small_cumulative_budget_rejects_one_centimeter",
        SmallCumulativeBudgetRejectsOneCentimeter);
    failures += RunTest("analysis_only_leaves_input_untouched", AnalysisOnlyLeavesInputUntouched);
    failures += RunTest("preexisting_glass_is_excluded", PreexistingGlassIsExcluded);
    failures += RunTest("single_box_has_no_self_conflict", SingleBoxHasNoSelfConflict);
    if (failures != 0) {
        std::printf("%d case(s) failed\n", failures);
        return 1;
    }
    std::printf("all 15 coplanar cleanup cases passed\n");
    return 0;
}
