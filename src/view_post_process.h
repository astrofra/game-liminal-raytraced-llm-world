#ifndef LIMINAL_VIEW_POST_PROCESS_H
#define LIMINAL_VIEW_POST_PROCESS_H

#include <stddef.h>
#include <stdio.h>
#include <vector>

namespace liminal {

struct PeripheralPostProcessConfig {
    bool enabled;
    float clear_radius;
    float full_effect_radius;
    int blur_radius_pixels;
    float blur_strength;
    float dispersion_pixels;
    float grain_strength;
    unsigned int grain_seed;
    bool debug_logging;

    PeripheralPostProcessConfig()
        : enabled(true)
        , clear_radius(0.48f)
        , full_effect_radius(1.05f)
        , blur_radius_pixels(3)
        , blur_strength(0.82f)
        , dispersion_pixels(6.0f)
        , grain_strength(7.0f)
        , grain_seed(0x6d2b79f5u)
        , debug_logging(false)
    {
    }
};

struct PeripheralPostProcessStats {
    size_t processed_pixel_count;
    size_t changed_pixel_count;
    size_t peripheral_changed_pixel_count;
    size_t grain_modified_pixel_count;
    float maximum_peripheral_weight;
    double duration_ms;

    PeripheralPostProcessStats()
        : processed_pixel_count(0u)
        , changed_pixel_count(0u)
        , peripheral_changed_pixel_count(0u)
        , grain_modified_pixel_count(0u)
        , maximum_peripheral_weight(0.0f)
        , duration_ms(0.0)
    {
    }
};

bool ApplyPeripheralViewPostProcess(
    std::vector<unsigned char>* rgb_pixels,
    int width,
    int height,
    const PeripheralPostProcessConfig& config,
    PeripheralPostProcessStats* stats = 0);

bool RunPeripheralViewPostProcessSelfTest(FILE* output);

}  // namespace liminal

#endif
