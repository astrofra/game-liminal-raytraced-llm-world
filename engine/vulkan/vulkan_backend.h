#pragma once
// Native Vulkan implementation of the E4B forward pass (Windows, Linux, and
// macOS through MoltenVK for development). Plain C++ interface: no Vulkan
// types leak to the engine core. The loader is opened at run time, so a
// machine without Vulkan only loses this device.
#include "core/e4b_spec.h"

#include <string>
#include <vector>

namespace liminal::engine {

struct VulkanModel;
struct VulkanState;

// Uploads the bound GGUF's weights to the first suitable GPU (K-quant blocks
// unchanged). LIMINAL_VULKAN_DEVICE=<index> picks another physical device.
VulkanModel* VulkanLoad(const E4bModel& model, std::string* error);
void VulkanFree(VulkanModel* model);
const char* VulkanDeviceName(const VulkanModel* model);

VulkanState* VulkanStateCreate(VulkanModel* model, int capacity, std::string* error);
void VulkanStateFree(VulkanState* state);

// Evaluates n tokens after the cached ones; `logits` receives the last token's
// softcapped logits (vocab floats).
bool VulkanForward(VulkanModel* model, VulkanState* state, const int32_t* tokens, int n,
    std::vector<float>* logits, std::string* error);

}  // namespace liminal::engine
