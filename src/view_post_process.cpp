#include "view_post_process.h"

#include <algorithm>
#include <chrono>
#include <math.h>

namespace liminal {

namespace {

static float ClampFloat(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

static int ClampInt(int value, int minimum, int maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

static float SmoothStep(float edge_zero, float edge_one, float value)
{
    if (edge_one <= edge_zero) {
        return value >= edge_one ? 1.0f : 0.0f;
    }
    const float t = ClampFloat((value - edge_zero) / (edge_one - edge_zero), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static unsigned char FloatToByte(float value)
{
    return static_cast<unsigned char>(ClampFloat(floorf(value + 0.5f), 0.0f, 255.0f));
}

static size_t PixelChannelIndex(int x, int y, int width, int channel)
{
    return (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u +
        static_cast<size_t>(channel);
}

static void BuildTriangularBlur(
    const std::vector<unsigned char>& source,
    int width,
    int height,
    int radius,
    std::vector<unsigned char>* blurred)
{
    if (!blurred) {
        return;
    }
    if (radius <= 0) {
        *blurred = source;
        return;
    }

    std::vector<unsigned char> horizontal(source.size(), 0u);
    blurred->assign(source.size(), 0u);
    const int weight_sum = (radius + 1) * (radius + 1);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                int sum = 0;
                for (int offset = -radius; offset <= radius; ++offset) {
                    const int sample_x = ClampInt(x + offset, 0, width - 1);
                    const int weight = radius + 1 - abs(offset);
                    sum += static_cast<int>(source[PixelChannelIndex(sample_x, y, width, channel)]) * weight;
                }
                horizontal[PixelChannelIndex(x, y, width, channel)] =
                    static_cast<unsigned char>((sum + weight_sum / 2) / weight_sum);
            }
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                int sum = 0;
                for (int offset = -radius; offset <= radius; ++offset) {
                    const int sample_y = ClampInt(y + offset, 0, height - 1);
                    const int weight = radius + 1 - abs(offset);
                    sum += static_cast<int>(horizontal[PixelChannelIndex(x, sample_y, width, channel)]) * weight;
                }
                (*blurred)[PixelChannelIndex(x, y, width, channel)] =
                    static_cast<unsigned char>((sum + weight_sum / 2) / weight_sum);
            }
        }
    }
}

static float SampleChannelBilinear(
    const std::vector<unsigned char>& pixels,
    int width,
    int height,
    float x,
    float y,
    int channel)
{
    const float clamped_x = ClampFloat(x, 0.0f, static_cast<float>(width - 1));
    const float clamped_y = ClampFloat(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(floorf(clamped_x));
    const int y0 = static_cast<int>(floorf(clamped_y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = clamped_x - static_cast<float>(x0);
    const float ty = clamped_y - static_cast<float>(y0);
    const float top =
        static_cast<float>(pixels[PixelChannelIndex(x0, y0, width, channel)]) * (1.0f - tx) +
        static_cast<float>(pixels[PixelChannelIndex(x1, y0, width, channel)]) * tx;
    const float bottom =
        static_cast<float>(pixels[PixelChannelIndex(x0, y1, width, channel)]) * (1.0f - tx) +
        static_cast<float>(pixels[PixelChannelIndex(x1, y1, width, channel)]) * tx;
    return top * (1.0f - ty) + bottom * ty;
}

static unsigned int MixGrainBits(unsigned int value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float SampleGrainNoise(int x, int y, int channel, unsigned int seed)
{
    unsigned int value = seed;
    value ^= static_cast<unsigned int>(x) * 0x9e3779b9u;
    value ^= static_cast<unsigned int>(y) * 0x85ebca6bu;
    value ^= static_cast<unsigned int>(channel + 1) * 0xc2b2ae35u;
    return static_cast<float>(MixGrainBits(value) & 0xffffu) / 32767.5f - 1.0f;
}

static bool BuffersEqual(const std::vector<unsigned char>& left, const std::vector<unsigned char>& right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

}  // namespace

bool ApplyPeripheralViewPostProcess(
    std::vector<unsigned char>* rgb_pixels,
    int width,
    int height,
    const PeripheralPostProcessConfig& config,
    PeripheralPostProcessStats* stats)
{
    if (stats) {
        *stats = PeripheralPostProcessStats();
    }
    if (!rgb_pixels || width <= 0 || height <= 0) {
        return false;
    }

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixel_count > static_cast<size_t>(-1) / 3u || rgb_pixels->size() != pixel_count * 3u) {
        return false;
    }
    if (!config.enabled) {
        return true;
    }

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const float clear_radius = ClampFloat(config.clear_radius, 0.0f, 2.0f);
    const float full_radius = std::max(clear_radius + 0.001f, ClampFloat(config.full_effect_radius, 0.0f, 2.0f));
    const int blur_radius = ClampInt(config.blur_radius_pixels, 0, 12);
    const float blur_strength = ClampFloat(config.blur_strength, 0.0f, 1.0f);
    const float maximum_dispersion = ClampFloat(config.dispersion_pixels, 0.0f, 16.0f);
    const float grain_strength = ClampFloat(config.grain_strength, 0.0f, 32.0f);
    const std::vector<unsigned char> source = *rgb_pixels;
    std::vector<unsigned char> blurred;
    BuildTriangularBlur(source, width, height, blur_radius, &blurred);

    const float center_x = static_cast<float>(width - 1) * 0.5f;
    const float center_y = static_cast<float>(height - 1) * 0.5f;
    const float inverse_half_width = 1.0f / std::max(center_x, 1.0f);
    const float inverse_half_height = 1.0f / std::max(center_y, 1.0f);
    size_t changed_pixel_count = 0u;
    size_t peripheral_changed_pixel_count = 0u;
    size_t grain_modified_pixel_count = 0u;
    float maximum_weight = 0.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float delta_x = static_cast<float>(x) - center_x;
            const float delta_y = static_cast<float>(y) - center_y;
            const float normalized_x = delta_x * inverse_half_width;
            const float normalized_y = delta_y * inverse_half_height;
            const float elliptical_radius = sqrtf(normalized_x * normalized_x + normalized_y * normalized_y);
            const float peripheral_weight = SmoothStep(clear_radius, full_radius, elliptical_radius);
            maximum_weight = std::max(maximum_weight, peripheral_weight);

            const float pixel_radius = sqrtf(delta_x * delta_x + delta_y * delta_y);
            const float radial_x = pixel_radius > 0.0001f ? delta_x / pixel_radius : 0.0f;
            const float radial_y = pixel_radius > 0.0001f ? delta_y / pixel_radius : 0.0f;
            const float dispersion = maximum_dispersion * peripheral_weight * peripheral_weight;
            const float blur_mix = blur_strength * peripheral_weight;

            float optical_values[3] = {0.0f, 0.0f, 0.0f};
            unsigned char optical_bytes[3] = {0u, 0u, 0u};
            bool peripheral_changed = false;
            for (int channel = 0; channel < 3; ++channel) {
                const float channel_direction = channel == 0 ? 1.0f : (channel == 2 ? -1.0f : 0.0f);
                const float sample_x = static_cast<float>(x) + radial_x * dispersion * channel_direction;
                const float sample_y = static_cast<float>(y) + radial_y * dispersion * channel_direction;
                const float sharp_value = SampleChannelBilinear(source, width, height, sample_x, sample_y, channel);
                const float blurred_value = SampleChannelBilinear(blurred, width, height, sample_x, sample_y, channel);
                optical_values[channel] = sharp_value * (1.0f - blur_mix) + blurred_value * blur_mix;
                optical_bytes[channel] = FloatToByte(optical_values[channel]);
                peripheral_changed = peripheral_changed ||
                    optical_bytes[channel] != source[PixelChannelIndex(x, y, width, channel)];
            }

            const float luminance =
                optical_values[0] * 0.2126f + optical_values[1] * 0.7152f + optical_values[2] * 0.0722f;
            const float midtone_weight = 1.0f - fabsf(luminance - 127.5f) / 127.5f;
            const float grain_envelope = 0.45f + 0.55f * ClampFloat(midtone_weight, 0.0f, 1.0f);
            const float common_grain = SampleGrainNoise(x, y, 3, config.grain_seed);
            bool grain_modified = false;
            bool pixel_changed = false;
            for (int channel = 0; channel < 3; ++channel) {
                const float channel_grain = SampleGrainNoise(x, y, channel, config.grain_seed);
                const float rgb_grain = grain_strength * grain_envelope *
                    (common_grain * 0.60f + channel_grain * 0.40f);
                const unsigned char output_value = FloatToByte(optical_values[channel] + rgb_grain);
                const size_t output_index = PixelChannelIndex(x, y, width, channel);
                (*rgb_pixels)[output_index] = output_value;
                grain_modified = grain_modified || output_value != optical_bytes[channel];
                pixel_changed = pixel_changed || output_value != source[output_index];
            }
            if (peripheral_changed) {
                ++peripheral_changed_pixel_count;
            }
            if (grain_modified) {
                ++grain_modified_pixel_count;
            }
            if (pixel_changed) {
                ++changed_pixel_count;
            }
        }
    }

    const double duration_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    if (stats) {
        stats->processed_pixel_count = pixel_count;
        stats->changed_pixel_count = changed_pixel_count;
        stats->peripheral_changed_pixel_count = peripheral_changed_pixel_count;
        stats->grain_modified_pixel_count = grain_modified_pixel_count;
        stats->maximum_peripheral_weight = maximum_weight;
        stats->duration_ms = duration_ms;
    }
    if (config.debug_logging) {
        fprintf(
            stdout,
            "[view-post-process] size=%dx%d blur_radius=%d blur_strength=%.2f dispersion=%.2f grain=%.2f optical_changed=%zu grain_changed=%zu total_changed=%zu/%zu duration_ms=%.2f\n",
            width,
            height,
            blur_radius,
            blur_strength,
            maximum_dispersion,
            grain_strength,
            peripheral_changed_pixel_count,
            grain_modified_pixel_count,
            changed_pixel_count,
            pixel_count,
            duration_ms);
        fflush(stdout);
    }
    return true;
}

bool RunPeripheralViewPostProcessSelfTest(FILE* output)
{
    PeripheralPostProcessConfig config;
    PeripheralPostProcessConfig optical_config = config;
    optical_config.grain_strength = 0.0f;
    const int width = 33;
    const int height = 33;
    std::vector<unsigned char> gradient(static_cast<size_t>(width * height * 3), 0u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char value = static_cast<unsigned char>((x * 255) / (width - 1));
            const size_t index = PixelChannelIndex(x, y, width, 0);
            gradient[index + 0] = value;
            gradient[index + 1] = value;
            gradient[index + 2] = value;
        }
    }

    std::vector<unsigned char> processed = gradient;
    std::vector<unsigned char> repeated = gradient;
    PeripheralPostProcessStats stats;
    if (!ApplyPeripheralViewPostProcess(&processed, width, height, optical_config, &stats) ||
        !ApplyPeripheralViewPostProcess(&repeated, width, height, optical_config, 0) ||
        !BuffersEqual(processed, repeated)) {
        if (output) {
            fprintf(output, "View post-process determinism test failed.\n");
        }
        return false;
    }

    const int center_x = width / 2;
    const int center_y = height / 2;
    const size_t center_index = PixelChannelIndex(center_x, center_y, width, 0);
    if (processed[center_index + 0] != gradient[center_index + 0] ||
        processed[center_index + 1] != gradient[center_index + 1] ||
        processed[center_index + 2] != gradient[center_index + 2]) {
        if (output) {
            fprintf(output, "View post-process clear-center test failed.\n");
        }
        return false;
    }

    const size_t edge_index = PixelChannelIndex(width - 2, center_y, width, 0);
    if (!(processed[edge_index + 0] > processed[edge_index + 1] &&
          processed[edge_index + 1] > processed[edge_index + 2]) ||
        stats.peripheral_changed_pixel_count == 0u ||
        stats.grain_modified_pixel_count != 0u ||
        stats.processed_pixel_count != static_cast<size_t>(width * height)) {
        if (output) {
            fprintf(output, "View post-process peripheral RGB-dispersion test failed.\n");
        }
        return false;
    }

    std::vector<unsigned char> checker(static_cast<size_t>(width * height * 3), 0u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char value = ((x + y) & 1) != 0 ? 255u : 0u;
            const size_t index = PixelChannelIndex(x, y, width, 0);
            checker[index + 0] = value;
            checker[index + 1] = value;
            checker[index + 2] = value;
        }
    }
    const std::vector<unsigned char> checker_reference = checker;
    PeripheralPostProcessConfig blur_only_config = optical_config;
    blur_only_config.dispersion_pixels = 0.0f;
    if (!ApplyPeripheralViewPostProcess(&checker, width, height, blur_only_config, 0)) {
        return false;
    }
    const size_t checker_center = PixelChannelIndex(center_x, center_y, width, 0);
    const size_t checker_edge = PixelChannelIndex(width - 2, center_y, width, 0);
    if (checker[checker_center] != checker_reference[checker_center] ||
        checker[checker_edge] == checker_reference[checker_edge] ||
        checker[checker_edge] <= 20u || checker[checker_edge] >= 235u) {
        if (output) {
            fprintf(output, "View post-process peripheral blur test failed.\n");
        }
        return false;
    }

    std::vector<unsigned char> uniform(static_cast<size_t>(width * height * 3), 0u);
    for (size_t index = 0; index < uniform.size(); index += 3u) {
        uniform[index + 0] = 42u;
        uniform[index + 1] = 100u;
        uniform[index + 2] = 201u;
    }
    const std::vector<unsigned char> uniform_reference = uniform;
    if (!ApplyPeripheralViewPostProcess(&uniform, width, height, optical_config, 0) ||
        !BuffersEqual(uniform, uniform_reference)) {
        if (output) {
            fprintf(output, "View post-process uniform-field preservation test failed.\n");
        }
        return false;
    }

    std::vector<unsigned char> grain_field(static_cast<size_t>(width * height * 3), 96u);
    std::vector<unsigned char> repeated_grain_field = grain_field;
    const std::vector<unsigned char> grain_reference = grain_field;
    PeripheralPostProcessConfig grain_only_config = config;
    grain_only_config.blur_radius_pixels = 0;
    grain_only_config.blur_strength = 0.0f;
    grain_only_config.dispersion_pixels = 0.0f;
    PeripheralPostProcessStats grain_stats;
    if (!ApplyPeripheralViewPostProcess(&grain_field, width, height, grain_only_config, &grain_stats) ||
        !ApplyPeripheralViewPostProcess(&repeated_grain_field, width, height, grain_only_config, 0) ||
        !BuffersEqual(grain_field, repeated_grain_field) ||
        BuffersEqual(grain_field, grain_reference) ||
        grain_stats.peripheral_changed_pixel_count != 0u ||
        grain_stats.grain_modified_pixel_count == 0u) {
        if (output) {
            fprintf(output, "View post-process post-optical grain determinism test failed.\n");
        }
        return false;
    }

    double grain_channel_sums[3] = {0.0, 0.0, 0.0};
    bool chromatic_grain_found = false;
    for (size_t index = 0; index < grain_field.size(); index += 3u) {
        for (int channel = 0; channel < 3; ++channel) {
            grain_channel_sums[channel] += static_cast<double>(grain_field[index + static_cast<size_t>(channel)]);
        }
        chromatic_grain_found = chromatic_grain_found ||
            grain_field[index + 0] != grain_field[index + 1] ||
            grain_field[index + 1] != grain_field[index + 2];
    }
    const double grain_pixel_count = static_cast<double>(width * height);
    if (!chromatic_grain_found ||
        fabs(grain_channel_sums[0] / grain_pixel_count - 96.0) > 1.5 ||
        fabs(grain_channel_sums[1] / grain_pixel_count - 96.0) > 1.5 ||
        fabs(grain_channel_sums[2] / grain_pixel_count - 96.0) > 1.5) {
        if (output) {
            fprintf(output, "View post-process RGB grain balance test failed.\n");
        }
        return false;
    }

    PeripheralPostProcessConfig disabled_config = config;
    disabled_config.enabled = false;
    std::vector<unsigned char> disabled = gradient;
    if (!ApplyPeripheralViewPostProcess(&disabled, width, height, disabled_config, 0) ||
        !BuffersEqual(disabled, gradient)) {
        if (output) {
            fprintf(output, "View post-process disabled bypass test failed.\n");
        }
        return false;
    }

    std::vector<unsigned char> invalid(3u, 0u);
    if (ApplyPeripheralViewPostProcess(&invalid, 2, 2, config, 0)) {
        if (output) {
            fprintf(output, "View post-process invalid-buffer rejection test failed.\n");
        }
        return false;
    }

    if (output) {
        fprintf(
            output,
            "View post-process self-test passed: deterministic optical pass, clear center, peripheral blur/RGB dispersion, post-optical RGB grain, bypass and validation.\n");
    }
    return true;
}

}  // namespace liminal
