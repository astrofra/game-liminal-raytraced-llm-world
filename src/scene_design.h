#ifndef LIMINAL_SCENE_DESIGN_H
#define LIMINAL_SCENE_DESIGN_H

#include "scene.h"
#include <string>
#include <vector>
#include <cstdint>

namespace liminal {

struct SceneDesignContext {
    std::string brief;
    std::string origin;
    std::string environment; // Read-only engine state and known invisible barriers.
    std::string language = "English";
    std::string entry_direction = "south"; // Side through which the player enters.
    std::vector<std::string> allowed_exits = {"north", "east", "south", "west"};
    std::vector<std::string> recent_intents;
};

struct DesignMass {
    std::string id, role, support;
    Vec3 center, size, rotation;
    std::string shape; // Authored assembly/opening description, optional in older plans.
};

struct DesignBinding { std::string object_id, mass_id; };
struct DesignPassage { std::string direction; Vec3 center, size; };

struct SceneDesignV1 {
    int version = 1;
    std::string title, intent, arrival, focal_object_id, lighting_mode, lighting_intent, continuity;
    Vec3 bounds_center, bounds_size;
    float floor_y = 0.0f;
    Camera camera;
    std::vector<DesignMass> masses;
    std::vector<DesignBinding> interaction_bindings;
    std::vector<DesignPassage> reserved_passages;
};

struct SceneDiagnostic {
    std::string code, object_id, detail;
};

struct SceneAuditReport {
    bool accepted = false;
    bool bounds_derived = false;
    std::vector<SceneDiagnostic> diagnostics;
    size_t triangles = 0, materials = 0, geometry_directives = 0;
    double focal_coverage = 0.0, near_coverage = 0.0, sky_coverage = 0.0;
    double mean_luminance = 0.0, luminance_deviation = 0.0;
    // Fixed opaque visibility pass. ID 0 is background; names are directive IDs,
    // independent of the number of materials produced by a prefab.
    std::vector<std::string> object_names;
    std::vector<uint16_t> object_ids;
    std::vector<float> depth;
    int landmark_projected_pixels = 0, landmark_occluded_pixels = 0;
};

std::string SceneDesignSchema();
std::string SerializeSceneDesign(const SceneDesignV1& design);
bool ParseSceneDesign(const std::string& text, const SceneDesignContext& context,
                      SceneDesignV1* design, SceneAuditReport* report);
std::string SerializeSceneAudit(const SceneAuditReport& report);
std::string SceneTextHash(const std::string& text);
std::string SceneCompositionGuide();
std::string BuildSceneDesignPrompt(const SceneDesignContext& context);
std::string BuildSceneProgramPrompt(const SceneDesignContext& context, const SceneDesignV1* design);
std::string LocalSceneSystemPrompt();
std::string LocalSceneRepairPrefix();
std::string LocalSceneHeader();
std::string LocalSceneExplorationBrief(unsigned int discovery_index);
std::string LocalScenePilotLandmark();
std::string BuildLocalSceneProgramPrompt(const std::string& brief,const std::string& landmark);
bool ValidateLocalSceneContract(const std::string& program,std::string* error);
// Fixed local camera: forward +Z, screen-right -X. Necessary clear approaches,
// not an automatic decision to open a route or proof of a continuous distant path.
std::vector<std::string> LocalSceneClearApproaches(const Scene& scene);
// Optional view contract for direct benchmarks; defaults preserve the dynamic path.
struct SceneViewConstraints {
    std::string landmark;
    float aspect = 2.0f;
    float near_distance = .35f;
    float max_near_coverage = .7f;
    bool require_return_clearance = false;
};
bool AuditDynamicScene(const std::string& program, const SceneDesignContext& context,
                       const SceneDesignV1* design, Scene* scene, SceneAuditReport* report,
                       const SceneViewConstraints* view = nullptr);
bool AuditDynamicPreview(const std::vector<unsigned char>& pixels, const std::string& lighting_mode,
                         SceneAuditReport* report);
bool RemoveLocalOccluder(const std::string& program, const std::string& landmark,
                         const SceneAuditReport& report, std::string* repaired, std::string* removed);
} // namespace liminal
#endif
