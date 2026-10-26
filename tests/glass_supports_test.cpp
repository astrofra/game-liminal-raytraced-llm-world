// DUO-032: expectations for the glass-support pilot pass, on the interface
// fixed by Codex in glass_supports.h (shipped with the production change).
//
// Scope: candidate detection, contact tolerance, projected footprint shape,
// documented omissions, opaque-source preservation and determinism of the pass
// output. Mesh construction details (shell welding, tilted underside envelope,
// duplicated corner faces) belong to Codex and are deliberately not asserted.
// No timing, no LLM call and no file access: every scene is inline.
//
// The fixtures under tests/data/glass_supports/ are synthetic pilot scenes for
// the before/after rendering; they are not reconstructed user captures.

#include "glass_supports.h"

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

// Minimal valid scene: room, camera, sky and a large ground plane. The camera
// looks at (0,2,5) from (0,2,-10), so it stays clear of every fixture.
const char* const kHeader =
R"SCENE(room "DUO-032 glass supports"
camera eye(0,2,-10) target(0,2,5) up(0,1,0) fov(50)
spotlight panel(1,1) offset(0,0,0.35) range(44) cone(15,36) intensity(58)
distant_light direction(0.75,-0.42,0.35) intensity(3.6) color(1.0,0.90,0.74) angular_radius(0.40)
sky zenith(0.04) horizon(0.35) nadir(0.00) band(0.34) curve(1.92) noise(0.12)
plane "desert_floor" pos(0,0,10) normal(0,1,0) size(120,160) gray(0.2)
)SCENE";

constexpr float kContactTolerance = 0.01f;
constexpr float kEmptyGapLimit = 0.05f;

std::string Program(const std::string& extra) { return std::string(kHeader) + extra; }

GlassSupportResult Build(const std::string& source, const GlassSupportOptions& options = GlassSupportOptions()) {
    return BuildGlassSupports(source, options);
}

void ExpectSuccess(const GlassSupportResult& result) {
    if (!result.success)
        throw std::runtime_error(std::string("BuildGlassSupports failed: ") + result.error);
}

bool HasKey(const nlohmann::json& object, const char* key) {
    return object.is_object() && object.find(key) != object.end();
}

bool IsIdentifier(const nlohmann::json& value) {
    return value.is_string() || value.is_number_integer();
}

std::string IdentifierText(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    throw std::runtime_error("identifier is neither a string nor an integer");
}

std::string BlockId(const nlohmann::json& block) { return IdentifierText(block.at("id")); }

void ValidateBlockSchema(const nlohmann::json& block) {
    CHECK(block.is_object());
    CHECK(HasKey(block, "id"));
    CHECK(IsIdentifier(block.at("id")));
    CHECK(HasKey(block, "min_y"));
    CHECK(block.at("min_y").is_number());
    CHECK(HasKey(block, "ground_contact"));
    CHECK(block.at("ground_contact").is_boolean() || block.at("ground_contact").is_number());
    CHECK(HasKey(block, "grounded"));
    CHECK(block.at("grounded").is_boolean());
    CHECK(HasKey(block, "component"));
    CHECK(block.at("component").is_number_integer());
    CHECK(HasKey(block, "neighbors"));
    CHECK(block.at("neighbors").is_array());
    for (const auto& neighbor : block.at("neighbors")) CHECK(IsIdentifier(neighbor));
    CHECK(HasKey(block, "footprint"));
    CHECK(block.at("footprint").is_array());
    for (const auto& point : block.at("footprint")) {
        CHECK(point.is_array());
        CHECK(point.size() == 2);
        CHECK(point[0].is_number());
        CHECK(point[1].is_number());
    }
    CHECK(HasKey(block, "decision"));
    CHECK(block.at("decision").is_string());
}

nlohmann::json ParseReport(const GlassSupportResult& result) {
    ExpectSuccess(result);
    nlohmann::json report;
    try {
        report = nlohmann::json::parse(result.report_json);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("invalid report_json: ") + error.what());
    }
    CHECK(report.is_object());
    CHECK(HasKey(report, "blocks"));
    CHECK(report.at("blocks").is_array());
    for (const auto& block : report.at("blocks")) ValidateBlockSchema(block);
    return report;
}

const nlohmann::json& FindBlock(const nlohmann::json& report, const char* id) {
    for (const auto& block : report.at("blocks"))
        if (BlockId(block) == id) return block;
    throw std::runtime_error(std::string("missing block in report: ") + id);
}

float MinY(const nlohmann::json& block) { return block.at("min_y").get<float>(); }
bool Grounded(const nlohmann::json& block) { return block.at("grounded").get<bool>(); }
long long Component(const nlohmann::json& block) { return block.at("component").get<long long>(); }
std::size_t FootprintSize(const nlohmann::json& block) { return block.at("footprint").size(); }

std::vector<std::string> NeighborIds(const nlohmann::json& block) {
    std::vector<std::string> ids;
    for (const auto& neighbor : block.at("neighbors")) ids.push_back(IdentifierText(neighbor));
    return ids;
}

bool ContainsId(const std::vector<std::string>& ids, const char* id) {
    for (const auto& value : ids)
        if (value == id) return true;
    return false;
}

bool Near(float value, float expected, float tolerance = 1.0e-3f) {
    return std::fabs(value - expected) <= tolerance;
}

void ExpectDecision(const nlohmann::json& block, const char* expected) {
    const std::string actual = block.at("decision").get<std::string>();
    if (actual != expected) {
        throw std::runtime_error("block " + BlockId(block) + ": decision '" + actual +
            "', expected '" + expected + "'");
    }
}

// World-space XZ bounds of an axis-aligned box turned around Y only. Used to
// prove that the two bars of the AABB-overlap case really do overlap.
struct PlanBounds {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
};

PlanBounds YawBounds(const Vec3& center, const Vec3& size, float yaw_degrees) {
    const float yaw = yaw_degrees * kPi / 180.0f;
    const float cosine = std::fabs(std::cos(yaw));
    const float sine = std::fabs(std::sin(yaw));
    const float half_x = 0.5f * (cosine * size.x + sine * size.z);
    const float half_z = 0.5f * (sine * size.x + cosine * size.z);
    return PlanBounds{center.x - half_x, center.x + half_x, center.z - half_z, center.z + half_z};
}

bool Overlap(const PlanBounds& a, const PlanBounds& b) {
    return a.min_x < b.max_x && b.min_x < a.max_x && a.min_z < b.max_z && b.min_z < a.max_z;
}

std::size_t CountSceneTriangles(const std::string& text, const char* scene_name) {
    Scene scene;
    char error_buffer[512] = {0};
    if (!LoadSceneFromSceneText(scene_name, text.c_str(), &scene, error_buffer, sizeof(error_buffer))) {
        throw std::runtime_error(std::string("LoadSceneFromSceneText failed for ") + scene_name +
            ": " + error_buffer);
    }
    return scene.triangles.size();
}

void GroundedCubeGetsNoSupport() {
    const std::string source =
        Program("box \"grounded_cube\" pos(0,0.5,5) size(2,1,2) gray(0.6)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(0));
    CHECK(result.added_triangles == std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& block = FindBlock(report, "grounded_cube");
    CHECK(Grounded(block));
    CHECK(Near(MinY(block), 0.0f, 1.0e-4f));
    ExpectDecision(block, "grounded");
}

void IsolatedCubeGetsOneSupport() {
    const std::string source =
        Program("box \"floating_cube\" pos(0,3,5) size(3,2,3) gray(0.55)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(1));
    CHECK(result.added_triangles > std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& block = FindBlock(report, "floating_cube");
    CHECK(!Grounded(block));
    CHECK(Near(MinY(block), 2.0f));
    ExpectDecision(block, "added");
}

void SlabOnGroundedPillarsIsSupported() {
    const std::string source = Program(
        "box \"pillar_west\" pos(-2.5,1,5) size(0.6,2,0.6) gray(0.5)\n"
        "box \"pillar_east\" pos(2.5,1,5) size(0.6,2,0.6) gray(0.5)\n"
        "box \"slab\" pos(0,2.2,5) size(6,0.4,3) gray(0.55)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    // The slab touches both pillar tops, so the ground anchoring propagates.
    CHECK(result.added_supports == std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& west = FindBlock(report, "pillar_west");
    const nlohmann::json& east = FindBlock(report, "pillar_east");
    const nlohmann::json& slab = FindBlock(report, "slab");
    CHECK(Grounded(west));
    CHECK(Grounded(east));
    ExpectDecision(west, "grounded");
    ExpectDecision(east, "grounded");
    CHECK(Grounded(slab));
    ExpectDecision(slab, "grounded");
    CHECK(Component(slab) == Component(west));
    CHECK(Component(slab) == Component(east));
    const std::vector<std::string> slab_neighbors = NeighborIds(slab);
    CHECK(ContainsId(slab_neighbors, "pillar_west"));
    CHECK(ContainsId(slab_neighbors, "pillar_east"));
}

void FloatingStackGetsASingleSupport() {
    const std::string source = Program(
        "box \"stack_bottom\" pos(0,2,5) size(2,1,2) gray(0.55)\n"
        "box \"stack_top\" pos(0,2.95,5) size(1.6,0.9,1.6) gray(0.6)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    // Candidates are handled from the lowest min_y, so the top block reuses the
    // component that the bottom support just anchored.
    CHECK(result.added_supports == std::size_t(1));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& bottom = FindBlock(report, "stack_bottom");
    const nlohmann::json& top = FindBlock(report, "stack_top");
    CHECK(!Grounded(bottom));
    CHECK(Near(MinY(bottom), 1.5f));
    ExpectDecision(bottom, "added");
    CHECK(!Grounded(top));
    ExpectDecision(top, "component_supported");
    CHECK(Component(bottom) == Component(top));
}

void OverlappingAabbsWithoutContactStaySeparate() {
    // Two 6 x 1 x 0.4 bars turned Y=45 and spaced 1.0 m apart, perpendicular to
    // their length: the free gap is 0.6 m while their world AABBs overlap.
    const Vec3 bar_size(6.0f, 1.0f, 0.4f);
    const Vec3 bar_a_center(0.0f, 3.0f, 5.0f);
    const Vec3 bar_b_center(0.7071f, 3.0f, 5.7071f);
    const std::string source = Program(
        "box \"bar_a\" pos(0,3,5) size(6,1,0.4) rot(0,45,0) gray(0.5)\n"
        "box \"bar_b\" pos(0.7071,3,5.7071) size(6,1,0.4) rot(0,45,0) gray(0.5)\n");
    CHECK(Overlap(YawBounds(bar_a_center, bar_size, 45.0f), YawBounds(bar_b_center, bar_size, 45.0f)));

    // Analysis only: no support is built, the report is the deliverable.
    GlassSupportOptions options;
    options.build_supports = false;
    const GlassSupportResult result = Build(source, options);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& a = FindBlock(report, "bar_a");
    const nlohmann::json& b = FindBlock(report, "bar_b");
    ExpectDecision(a, "candidate");
    ExpectDecision(b, "candidate");
    CHECK(NeighborIds(a).empty());
    CHECK(NeighborIds(b).empty());
    CHECK(Component(a) != Component(b));
}

void YawRotationKeepsARectangularFootprint() {
    const std::string source =
        Program("box \"yaw_mass\" pos(0,3,5) size(5,1.5,3) rot(0,38,0) gray(0.55)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(1));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& block = FindBlock(report, "yaw_mass");
    CHECK(Near(MinY(block), 2.25f));
    CHECK(FootprintSize(block) == std::size_t(4));
    ExpectDecision(block, "added");
}

void TiltedRotationProducesASixVertexFootprint() {
    const std::string source =
        Program("box \"tilted_mass\" pos(0,3,5) size(3,4,3) rot(19,27,13) gray(0.55)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(1));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& block = FindBlock(report, "tilted_mass");
    CHECK(!Grounded(block));
    CHECK(MinY(block) > kEmptyGapLimit);
    CHECK(FootprintSize(block) == std::size_t(6));
    ExpectDecision(block, "added");
}

void ContactToleranceBoundariesAreStable() {
    const std::string source = Program(
        "box \"contact_009\" pos(-9,0.509,8) size(2,1,2) gray(0.5)\n"
        "box \"gap_011\" pos(-3,0.511,8) size(2,1,2) gray(0.5)\n"
        "box \"gap_049\" pos(3,0.549,8) size(2,1,2) gray(0.5)\n"
        "box \"supported_051\" pos(9,0.551,8) size(2,1,2) gray(0.5)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    // The contact tolerance is inclusive at 0.01 m; gaps up to 0.05 m are noise.
    CHECK(result.added_supports == std::size_t(1));
    const nlohmann::json report = ParseReport(result);
    const nlohmann::json& contact = FindBlock(report, "contact_009");
    CHECK(Grounded(contact));
    CHECK(MinY(contact) <= kContactTolerance);
    ExpectDecision(contact, "grounded");

    const nlohmann::json& noise_011 = FindBlock(report, "gap_011");
    CHECK(!Grounded(noise_011));
    CHECK(MinY(noise_011) > kContactTolerance);
    CHECK(MinY(noise_011) <= kEmptyGapLimit);
    ExpectDecision(noise_011, "below_gap");

    const nlohmann::json& noise_049 = FindBlock(report, "gap_049");
    CHECK(!Grounded(noise_049));
    CHECK(MinY(noise_049) > kContactTolerance);
    CHECK(MinY(noise_049) <= kEmptyGapLimit);
    ExpectDecision(noise_049, "below_gap");

    const nlohmann::json& supported = FindBlock(report, "supported_051");
    CHECK(!Grounded(supported));
    CHECK(MinY(supported) > kEmptyGapLimit);
    ExpectDecision(supported, "added");
}

void NarrowFootprintIsOmitted() {
    // 0.4 m across: two 0.25 m walls would leave no usable interior.
    const std::string source =
        Program("box \"narrow_mass\" pos(0,3,5) size(0.4,2,2) gray(0.5)\n");
    const GlassSupportResult result = Build(source);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(0));
    CHECK(result.added_triangles == std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    ExpectDecision(FindBlock(report, "narrow_mass"), "narrow_footprint");
}

void ReservedVolumeIsOmitted() {
    const std::string source =
        Program("box \"reserved_wall\" pos(0,3,5) size(6,2,2) gray(0.5)\n");
    GlassSupportOptions options;
    options.protect_local_approaches = true;
    options.reserved.push_back(GlassSupportExclusion{
        "camera_approach", Vec3(0.0f, 1.0f, 5.0f), Vec3(8.0f, 3.0f, 3.0f)});
    const GlassSupportResult result = Build(source, options);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(0));
    const nlohmann::json report = ParseReport(result);
    ExpectDecision(FindBlock(report, "reserved_wall"), "reserved_volume");
}

void TriangleBudgetKeepsTheOpaqueScene() {
    const std::string source =
        Program("box \"budget_mass\" pos(0,3,5) size(3,2,3) gray(0.5)\n");
    GlassSupportOptions options;
    options.max_triangles = 1;
    const GlassSupportResult result = Build(source, options);
    ExpectSuccess(result);
    CHECK(result.added_supports == std::size_t(0));
    CHECK(result.added_triangles == std::size_t(0));
    // Nothing was built, so the validated opaque scene is returned untouched.
    CHECK(result.opaque_scene_text == source);
    CHECK(result.scene_text == result.opaque_scene_text);
    const nlohmann::json report = ParseReport(result);
    ExpectDecision(FindBlock(report, "budget_mass"), "triangle_budget");
}

void OutputIsDeterministicAndReloadable() {
    const std::string source =
        Program("box \"yaw_mass\" pos(0,3,5) size(5,1.5,3) rot(0,38,0) gray(0.5)\n");
    const GlassSupportResult first = Build(source);
    const GlassSupportResult second = Build(source);
    ExpectSuccess(first);
    ExpectSuccess(second);
    // The opaque input is never rewritten by the pass.
    CHECK(first.opaque_scene_text == source);
    CHECK(second.opaque_scene_text == source);
    CHECK(first.scene_text == second.scene_text);
    CHECK(first.added_supports == second.added_supports);
    CHECK(first.added_supports == std::size_t(1));

    // A second pass over its own output is idempotent: no stacked supports.
    const GlassSupportResult reapplied = Build(first.scene_text);
    ExpectSuccess(reapplied);
    CHECK(reapplied.scene_text == first.scene_text);
    CHECK(reapplied.added_supports == first.added_supports);

    // The produced text reloads, and the triangles it adds match the report.
    const std::size_t opaque_triangles = CountSceneTriangles(source, "duo032_opaque");
    const std::size_t augmented_triangles =
        CountSceneTriangles(first.scene_text, "duo032_augmented");
    const std::size_t reloaded_triangles =
        CountSceneTriangles(second.scene_text, "duo032_reloaded");
    CHECK(augmented_triangles > opaque_triangles);
    CHECK(augmented_triangles == opaque_triangles + first.added_triangles);
    CHECK(reloaded_triangles == augmented_triangles);
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

}  // namespace

int main() {
    int failures = 0;
    failures += Run("grounded_cube_gets_no_support", GroundedCubeGetsNoSupport);
    failures += Run("isolated_cube_gets_one_support", IsolatedCubeGetsOneSupport);
    failures += Run("slab_on_grounded_pillars_is_supported", SlabOnGroundedPillarsIsSupported);
    failures += Run("floating_stack_gets_a_single_support", FloatingStackGetsASingleSupport);
    failures += Run("overlapping_aabbs_without_contact_stay_separate",
        OverlappingAabbsWithoutContactStaySeparate);
    failures += Run("yaw_rotation_keeps_a_rectangular_footprint",
        YawRotationKeepsARectangularFootprint);
    failures += Run("tilted_rotation_produces_a_six_vertex_footprint",
        TiltedRotationProducesASixVertexFootprint);
    failures += Run("contact_tolerance_boundaries_are_stable", ContactToleranceBoundariesAreStable);
    failures += Run("narrow_footprint_is_omitted", NarrowFootprintIsOmitted);
    failures += Run("reserved_volume_is_omitted", ReservedVolumeIsOmitted);
    failures += Run("triangle_budget_keeps_the_opaque_scene", TriangleBudgetKeepsTheOpaqueScene);
    failures += Run("output_is_deterministic_and_reloadable", OutputIsDeterministicAndReloadable);
    if (failures != 0) {
        std::printf("%d case(s) failed\n", failures);
        return 1;
    }
    std::printf("all 12 glass support cases passed\n");
    return 0;
}
