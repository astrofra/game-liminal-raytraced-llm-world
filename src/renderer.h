#ifndef LIMINAL_RENDERER_RENDERER_H
#define LIMINAL_RENDERER_RENDERER_H

#include "scene.h"

namespace liminal {

struct RenderControl {
    // Return false to cancel. Calls are serialized even with OpenMP enabled.
    bool (*continue_render)(void*) = nullptr;
    void* user = nullptr;
};
bool ProbeSceneRay(const Scene& scene, const Ray& ray, float max_distance, Hit* hit);
bool RenderSceneToPixels(const Scene& scene, const RenderConfig& config, std::vector<unsigned char>* pixels,
                         const RenderControl* control = nullptr);
bool RenderSceneToImage(const Scene& scene, const RenderConfig& config, const char* output_path,
                       const RenderControl* control = nullptr);

}  // namespace liminal

#endif
