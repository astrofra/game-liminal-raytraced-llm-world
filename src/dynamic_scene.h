#ifndef LIMINAL_DYNAMIC_SCENE_H
#define LIMINAL_DYNAMIC_SCENE_H

#include "scene_design.h"
#include "llm_runtime.h"
#include <functional>

namespace liminal {

enum class DynamicScenePhase { Design, Program, Repair, Audit, Preview };
using DynamicSceneProgress = bool (*)(DynamicScenePhase, const char*, const char*, void*);
using SceneCompletion = std::function<bool(const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&,
                                          LlmStreamCallback, void*, LlmGenerationResult*)>;

struct DynamicSceneConfig {
    int plan_tokens = 2304;
    int program_tokens = 4096;
    int max_repairs = 2;
    int total_timeout_ms = 900000; // Safety bound, not an acceptance target for latency.
    bool direct = false; // Benchmark baseline only; the game's dynamic mode uses a plan.
    bool fixed_frame = false; // Local roadmap pilot: shared benchmark contract, no plan pass.
    std::string landmark = "datum";
    SceneCompletion completion; // Optional test seam; empty selects the real native runtime.
};

struct DynamicSceneResult {
    bool success = false, cancelled = false;
    SceneDesignV1 design;
    Scene scene;
    SceneAuditReport audit;
    std::string plan_json, scene_text, scene_hash, error, metrics_json;
    int repairs = 0, calls = 0;
    std::vector<unsigned char> preview_pixels;
    struct Attempt {
        std::string raw, normalized, final_program, error;
        unsigned int seed = 0;
        std::vector<std::string> normalizations, removed_objects;
        std::vector<std::string> repair_programs;
        SceneAuditReport before, after;
    };
    std::vector<Attempt> attempts;
};

bool GenerateDynamicScene(const SceneDesignContext& context, const LlmGenerationConfig& model,
                          const DynamicSceneConfig& config, DynamicSceneProgress progress, void* user,
                          DynamicSceneResult* result);
} // namespace liminal
#endif
