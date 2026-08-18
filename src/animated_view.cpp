#include "animated_view.h"

#include <algorithm>
#include <math.h>
#include <utility>

namespace liminal {

namespace {

struct CameraBasis {
    Vec3 forward;
    Vec3 right;
    Vec3 up;
};

static CameraBasis BuildCameraBasis(const Camera& camera)
{
    CameraBasis basis;
    basis.forward = Normalize(camera.target - camera.eye);
    basis.right = Normalize(Cross(basis.forward, camera.up));
    basis.up = Normalize(Cross(basis.right, basis.forward));
    return basis;
}

static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
{
    return a + (b - a) * t;
}

static Vec3 RotateAroundAxis(const Vec3& value, const Vec3& axis, float radians)
{
    const Vec3 normalized_axis = Normalize(axis);
    const float cosine = cosf(radians);
    const float sine = sinf(radians);
    return value * cosine + Cross(normalized_axis, value) * sine +
        normalized_axis * Dot(normalized_axis, value) * (1.0f - cosine);
}

static bool NearlyEqual(float a, float b, float epsilon)
{
    return fabsf(a - b) <= epsilon;
}

static bool SameVec3(const Vec3& a, const Vec3& b, float epsilon)
{
    return NearlyEqual(a.x, b.x, epsilon) &&
        NearlyEqual(a.y, b.y, epsilon) &&
        NearlyEqual(a.z, b.z, epsilon);
}

static bool SameCamera(const Camera& a, const Camera& b, float epsilon)
{
    return SameVec3(a.eye, b.eye, epsilon) &&
        SameVec3(a.target, b.target, epsilon) &&
        SameVec3(a.up, b.up, epsilon) &&
        NearlyEqual(a.vertical_fov_degrees, b.vertical_fov_degrees, epsilon);
}

static bool ExpectSequence(
    int image_count,
    const int* expected,
    size_t expected_count,
    FILE* output)
{
    Scene scene;
    AnimatedViewConfig config;
    AnimatedView view;
    std::vector<unsigned char> first_pixels(3u, 0u);
    if (!InitializeAnimatedView(&view, 1u, scene, config, &first_pixels, 0.0)) {
        return false;
    }

    for (int index = 1; index < image_count; ++index) {
        std::vector<unsigned char> pixels(3u, static_cast<unsigned char>(index));
        if (!AppendAnimatedViewImage(&view, view.generation_id, index, &pixels, 0.0)) {
            return false;
        }
    }

    const double step_seconds = 1.0 / static_cast<double>(config.playback_images_per_second);
    for (size_t position = 0; position < expected_count; ++position) {
        if (view.displayed_image_index != expected[position]) {
            if (output) {
                fprintf(
                    output,
                    "Animated view sequence mismatch for N=%d at position %zu: expected %d, got %d\n",
                    image_count,
                    position,
                    expected[position],
                    view.displayed_image_index);
            }
            return false;
        }
        AdvanceAnimatedViewPlayback(&view, step_seconds + 1.0e-7, config.playback_images_per_second);
    }
    return true;
}

}  // namespace

Camera BuildBreathingPoseB(const Camera& pose_a, const AnimatedViewConfig& config)
{
    const CameraBasis authored_basis = BuildCameraBasis(pose_a);
    Vec3 forward = authored_basis.forward;
    Vec3 up = authored_basis.up;
    Vec3 right = authored_basis.right;
    const float degrees_to_radians = kPi / 180.0f;

    if (fabsf(config.yaw_degrees) > kEpsilon) {
        forward = Normalize(RotateAroundAxis(forward, up, config.yaw_degrees * degrees_to_radians));
        right = Normalize(Cross(forward, up));
        up = Normalize(Cross(right, forward));
    }
    if (fabsf(config.pitch_degrees) > kEpsilon) {
        forward = Normalize(RotateAroundAxis(forward, right, config.pitch_degrees * degrees_to_radians));
        up = Normalize(RotateAroundAxis(up, right, config.pitch_degrees * degrees_to_radians));
    }
    if (fabsf(config.roll_degrees) > kEpsilon) {
        up = Normalize(RotateAroundAxis(up, forward, config.roll_degrees * degrees_to_radians));
    }
    right = Normalize(Cross(forward, up));
    up = Normalize(Cross(right, forward));

    const Vec3 translation =
        authored_basis.up * config.vertical_translation_m +
        authored_basis.forward * config.forward_translation_m;
    const float focus_distance = std::max(Length(pose_a.target - pose_a.eye), kEpsilon);

    Camera pose_b = pose_a;
    pose_b.eye = pose_a.eye + translation;
    pose_b.target = pose_b.eye + forward * focus_distance;
    pose_b.up = up;
    return pose_b;
}

Camera CalculateAnimatedCameraPose(const Camera& pose_a, const Camera& pose_b, int image_index)
{
    if (image_index <= 0) {
        return pose_a;
    }
    if (image_index >= kAnimatedViewImageCapacity - 1) {
        return pose_b;
    }

    const float t = static_cast<float>(image_index) /
        static_cast<float>(kAnimatedViewImageCapacity - 1);
    const float eased_t = 0.5f - 0.5f * cosf(kPi * t);
    const CameraBasis basis_a = BuildCameraBasis(pose_a);
    const CameraBasis basis_b = BuildCameraBasis(pose_b);

    Vec3 forward = Normalize(Lerp(basis_a.forward, basis_b.forward, eased_t));
    Vec3 up = Normalize(Lerp(basis_a.up, basis_b.up, eased_t));
    const Vec3 right = Normalize(Cross(forward, up));
    up = Normalize(Cross(right, forward));

    const float focus_distance_a = Length(pose_a.target - pose_a.eye);
    const float focus_distance_b = Length(pose_b.target - pose_b.eye);
    const float focus_distance =
        focus_distance_a + (focus_distance_b - focus_distance_a) * eased_t;

    Camera pose = pose_a;
    pose.eye = Lerp(pose_a.eye, pose_b.eye, eased_t);
    pose.target = pose.eye + forward * focus_distance;
    pose.up = up;
    pose.vertical_fov_degrees = pose_a.vertical_fov_degrees;
    return pose;
}

bool InitializeAnimatedView(
    AnimatedView* view,
    uint64_t generation_id,
    const Scene& scene_snapshot,
    const AnimatedViewConfig& config,
    std::vector<unsigned char>* first_image_pixels,
    double first_image_render_duration_ms)
{
    if (!view || !first_image_pixels || first_image_pixels->empty()) {
        return false;
    }

    AnimatedView initialized;
    initialized.generation_id = generation_id;
    initialized.scene_snapshot = scene_snapshot;
    initialized.pose_a = scene_snapshot.camera;
    initialized.pose_b = BuildBreathingPoseB(initialized.pose_a, config);
    initialized.total_generation_duration_ms = std::max(0.0, first_image_render_duration_ms);

    AnimationImage first_image;
    first_image.pixels.swap(*first_image_pixels);
    first_image.render_duration_ms = std::max(0.0, first_image_render_duration_ms);
    initialized.images.push_back(AnimationImage());
    initialized.images.back().pixels.swap(first_image.pixels);
    initialized.images.back().render_duration_ms = first_image.render_duration_ms;
    initialized.available_image_count = 1;

    *view = std::move(initialized);
    return true;
}

bool AppendAnimatedViewImage(
    AnimatedView* view,
    uint64_t generation_id,
    int image_index,
    std::vector<unsigned char>* pixels,
    double render_duration_ms)
{
    if (!view || generation_id != view->generation_id || !pixels || pixels->empty() ||
        image_index != view->available_image_count ||
        image_index < 0 || image_index >= kAnimatedViewImageCapacity) {
        return false;
    }

    view->images.push_back(AnimationImage());
    view->images.back().pixels.swap(*pixels);
    view->images.back().render_duration_ms = std::max(0.0, render_duration_ms);
    view->available_image_count = static_cast<int>(view->images.size());
    view->total_generation_duration_ms += std::max(0.0, render_duration_ms);
    return true;
}

void AdvanceAnimatedViewPlayback(AnimatedView* view, double elapsed_seconds, float images_per_second)
{
    if (!view || view->available_image_count <= 1 || images_per_second <= 0.0f) {
        if (view) {
            view->displayed_image_index = 0;
            view->playback_direction = 1;
            view->playback_accumulator_seconds = 0.0;
        }
        return;
    }

    const double bounded_elapsed = std::max(0.0, std::min(elapsed_seconds, 0.5));
    const double seconds_per_image = 1.0 / static_cast<double>(images_per_second);
    view->playback_accumulator_seconds += bounded_elapsed;

    while (view->playback_accumulator_seconds + 1.0e-9 >= seconds_per_image) {
        view->playback_accumulator_seconds -= seconds_per_image;
        if (view->playback_direction > 0) {
            if (view->displayed_image_index + 1 < view->available_image_count) {
                ++view->displayed_image_index;
            } else {
                view->playback_direction = -1;
                view->displayed_image_index = std::max(0, view->displayed_image_index - 1);
            }
        } else if (view->displayed_image_index > 0) {
            --view->displayed_image_index;
        } else {
            view->playback_direction = 1;
            view->displayed_image_index = std::min(
                view->available_image_count - 1,
                view->displayed_image_index + 1);
        }
    }
}

size_t AnimatedViewImageMemoryBytes(const AnimatedView& view)
{
    size_t bytes = 0;
    for (size_t index = 0; index < view.images.size(); ++index) {
        bytes += view.images[index].pixels.size();
    }
    return bytes;
}

bool RunAnimatedViewSelfTest(FILE* output)
{
    const int sequence_one[] = {0, 0, 0, 0};
    const int sequence_two[] = {0, 1, 0, 1, 0, 1};
    const int sequence_four[] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3};
    const int sequence_eight[] = {0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1};
    if (!ExpectSequence(1, sequence_one, sizeof(sequence_one) / sizeof(sequence_one[0]), output) ||
        !ExpectSequence(2, sequence_two, sizeof(sequence_two) / sizeof(sequence_two[0]), output) ||
        !ExpectSequence(4, sequence_four, sizeof(sequence_four) / sizeof(sequence_four[0]), output) ||
        !ExpectSequence(8, sequence_eight, sizeof(sequence_eight) / sizeof(sequence_eight[0]), output)) {
        return false;
    }

    Scene stale_test_scene;
    AnimatedView stale_test_view;
    AnimatedViewConfig stale_test_config;
    std::vector<unsigned char> stale_test_first_pixels(3u, 0u);
    std::vector<unsigned char> stale_pixels(3u, 1u);
    if (!InitializeAnimatedView(
            &stale_test_view,
            10u,
            stale_test_scene,
            stale_test_config,
            &stale_test_first_pixels,
            0.0) ||
        AppendAnimatedViewImage(&stale_test_view, 9u, 1, &stale_pixels, 0.0) ||
        stale_test_view.available_image_count != 1) {
        if (output) {
            fprintf(output, "Animated view stale-generation rejection test failed.\n");
        }
        return false;
    }
    for (int image_index = 1; image_index < kAnimatedViewImageCapacity; ++image_index) {
        std::vector<unsigned char> pixels(3u, static_cast<unsigned char>(image_index));
        if (!AppendAnimatedViewImage(&stale_test_view, 10u, image_index, &pixels, 0.0)) {
            return false;
        }
    }
    std::vector<unsigned char> overflow_pixels(3u, 255u);
    if (AppendAnimatedViewImage(
            &stale_test_view,
            10u,
            kAnimatedViewImageCapacity,
            &overflow_pixels,
            0.0) ||
        stale_test_view.available_image_count != kAnimatedViewImageCapacity) {
        if (output) {
            fprintf(output, "Animated view capacity guard test failed.\n");
        }
        return false;
    }

    Camera pose_a;
    pose_a.eye = Vec3(1.0f, 2.0f, 3.0f);
    pose_a.target = Vec3(1.0f, 2.0f, 8.0f);
    pose_a.up = Vec3(0.0f, 1.0f, 0.0f);
    pose_a.vertical_fov_degrees = 47.0f;
    AnimatedViewConfig config;
    const Camera pose_b = BuildBreathingPoseB(pose_a, config);
    const Camera image_zero = CalculateAnimatedCameraPose(pose_a, pose_b, 0);
    const Camera image_three = CalculateAnimatedCameraPose(pose_a, pose_b, 3);
    const Camera image_seven = CalculateAnimatedCameraPose(pose_a, pose_b, 7);
    const CameraBasis pose_a_basis = BuildCameraBasis(pose_a);
    const Vec3 endpoint_translation = pose_b.eye - pose_a.eye;
    const float image_three_t = 3.0f / 7.0f;
    const float image_three_eased_t = 0.5f - 0.5f * cosf(kPi * image_three_t);
    const Vec3 expected_image_three_eye = pose_a.eye + endpoint_translation * image_three_eased_t;
    if (!SameCamera(image_zero, pose_a, 0.0f) ||
        !SameCamera(image_seven, pose_b, 0.0f) ||
        !NearlyEqual(image_seven.vertical_fov_degrees, pose_a.vertical_fov_degrees, 0.0f) ||
        !NearlyEqual(Dot(endpoint_translation, pose_a_basis.up), config.vertical_translation_m, 1.0e-5f) ||
        !NearlyEqual(Dot(endpoint_translation, pose_a_basis.forward), config.forward_translation_m, 1.0e-5f) ||
        !NearlyEqual(Dot(endpoint_translation, pose_a_basis.right), 0.0f, 1.0e-5f) ||
        !SameVec3(image_three.eye, expected_image_three_eye, 1.0e-5f)) {
        if (output) {
            fprintf(output, "Animated view endpoint camera test failed.\n");
        }
        return false;
    }

    for (int cycle = 0; cycle < 10; ++cycle) {
        if (!SameCamera(CalculateAnimatedCameraPose(pose_a, pose_b, 0), pose_a, 0.0f) ||
            !SameCamera(CalculateAnimatedCameraPose(pose_a, pose_b, 7), pose_b, 0.0f)) {
            if (output) {
                fprintf(output, "Animated view camera drift test failed at cycle %d.\n", cycle + 1);
            }
            return false;
        }
    }

    if (output) {
        fprintf(
            output,
            "Animated view self-test passed: N=1/2/4/8 ping-pong, capacity, easing, exact endpoints, no drift, stale rejection.\n");
    }
    return true;
}

}  // namespace liminal
