#ifndef LIMINAL_COPLANAR_CLEANUP_H
#define LIMINAL_COPLANAR_CLEANUP_H
#include "scene.h"
#include <functional>
namespace liminal {
struct CoplanarCleanupOptions {
    bool apply_translations = true;
    bool protect_local_approaches = true;
    double facing_min = .05;
    double angle_tolerance_degrees = .1;
    double plane_tolerance = .001;
    double overlap_area_min = 1e-6;
    size_t max_translations = 16;
    size_t max_candidate_trials = 256;
    double max_cumulative_distance = .05;
    std::vector<std::string> fixed_ids;
    std::vector<Camera> extra_cameras;
    // Additional authoritative audit; every candidate remains provisional until
    // it passes. false rolls back the entire trial, including serialized position.
    std::function<bool(const std::string&,std::string*)> validate;
};
struct CoplanarCleanupResult {
    bool success = false;
    std::string error, source_scene_text, scene_text, report_json;
    size_t initial_conflicts = 0, remaining_conflicts = 0, accepted_translations = 0;
};
// Returns opaque geometry only: any existing engine supports are invalidated.
// Processed scenes carry a content-bound marker, including unresolved cases.
CoplanarCleanupResult CleanupCoplanarFaces(const std::string& program,
                                         const CoplanarCleanupOptions& options = {});
} // namespace liminal
#endif
