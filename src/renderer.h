#ifndef LIMINAL_RENDERER_RENDERER_H
#define LIMINAL_RENDERER_RENDERER_H

#include "scene.h"

namespace liminal {

bool RenderSceneToPixels(const Scene& scene, const RenderConfig& config, std::vector<unsigned char>* pixels);
bool RenderSceneToImage(const Scene& scene, const RenderConfig& config, const char* output_path);

}  // namespace liminal

#endif
