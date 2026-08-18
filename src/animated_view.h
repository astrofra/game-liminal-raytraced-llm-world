#ifndef LIMINAL_RENDERER_ANIMATED_VIEW_H
#define LIMINAL_RENDERER_ANIMATED_VIEW_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "scene.h"

namespace liminal {

static const int kAnimatedViewImageCapacity = 8;

struct AnimatedViewConfig {
    float vertical_translation_m;
    float forward_translation_m;
    float pitch_degrees;
    float roll_degrees;
    float yaw_degrees;
    float playback_images_per_second;
    int forced_failure_image_index;
    bool debug_logging;

    AnimatedViewConfig()
        : vertical_translation_m(0.025f)
        , forward_translation_m(0.012f)
        , pitch_degrees(0.35f)
        , roll_degrees(0.15f)
        , yaw_degrees(0.0f)
        , playback_images_per_second(6.0f)
        , forced_failure_image_index(-1)
        , debug_logging(false)
    {
    }
};

struct AnimationImage {
    std::vector<unsigned char> pixels;
    double render_duration_ms;

    AnimationImage()
        : render_duration_ms(0.0)
    {
    }
};

struct AnimatedView {
    uint64_t generation_id;
    Scene scene_snapshot;
    Camera pose_a;
    Camera pose_b;
    std::vector<AnimationImage> images;
    int available_image_count;
    int image_being_rendered;
    int displayed_image_index;
    int playback_direction;
    double playback_accumulator_seconds;
    bool generation_complete;
    bool generation_failed;
    double total_generation_duration_ms;

    AnimatedView()
        : generation_id(0)
        , available_image_count(0)
        , image_being_rendered(-1)
        , displayed_image_index(0)
        , playback_direction(1)
        , playback_accumulator_seconds(0.0)
        , generation_complete(false)
        , generation_failed(false)
        , total_generation_duration_ms(0.0)
    {
        images.reserve(kAnimatedViewImageCapacity);
    }
};

Camera BuildBreathingPoseB(const Camera& pose_a, const AnimatedViewConfig& config);
Camera CalculateAnimatedCameraPose(const Camera& pose_a, const Camera& pose_b, int image_index);

bool InitializeAnimatedView(
    AnimatedView* view,
    uint64_t generation_id,
    const Scene& scene_snapshot,
    const AnimatedViewConfig& config,
    std::vector<unsigned char>* first_image_pixels,
    double first_image_render_duration_ms);

bool AppendAnimatedViewImage(
    AnimatedView* view,
    uint64_t generation_id,
    int image_index,
    std::vector<unsigned char>* pixels,
    double render_duration_ms);

void AdvanceAnimatedViewPlayback(AnimatedView* view, double elapsed_seconds, float images_per_second);
size_t AnimatedViewImageMemoryBytes(const AnimatedView& view);
bool RunAnimatedViewSelfTest(FILE* output);

}  // namespace liminal

#endif
