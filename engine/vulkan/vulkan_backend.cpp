#include "vulkan/vulkan_backend.h"

#include "core/quant_ref.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace liminal::engine {

namespace {

// SPIR-V compiled from engine/vulkan/shaders by glslc at build time.
const uint32_t kSpvGemvQ4k[] = {
#include "gemv_q4k.spv.inc"
};
const uint32_t kSpvGemvQ6k[] = {
#include "gemv_q6k.spv.inc"
};
const uint32_t kSpvGemvF16[] = {
#include "gemv_f16.spv.inc"
};
const uint32_t kSpvDequantQ4k[] = {
#include "dequant_q4k.spv.inc"
};
const uint32_t kSpvDequantQ6k[] = {
#include "dequant_q6k.spv.inc"
};
const uint32_t kSpvGemm[] = {
#include "gemm.spv.inc"
};
const uint32_t kSpvRmsNorm[] = {
#include "rms_norm.spv.inc"
};
const uint32_t kSpvAddNorm[] = {
#include "add_norm.spv.inc"
};
const uint32_t kSpvQkvPrep[] = {
#include "qkv_prep.spv.inc"
};
const uint32_t kSpvAttnDecode[] = {
#include "attn_decode.spv.inc"
};
const uint32_t kSpvAttnReduce[] = {
#include "attn_reduce.spv.inc"
};
const uint32_t kSpvSoftmaxMask[] = {
#include "softmax_mask.spv.inc"
};
const uint32_t kSpvGeglu[] = {
#include "geglu.spv.inc"
};
const uint32_t kSpvPlePrep[] = {
#include "ple_prep.spv.inc"
};

// Every Vulkan entry point the backend uses, resolved from the loader at run time.
#define LIMINAL_VK_INSTANCE_FUNCS(X) \
    X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceFeatures2) X(vkGetPhysicalDeviceQueueFamilyProperties) \
    X(vkGetPhysicalDeviceMemoryProperties) X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) \
    X(vkGetDeviceProcAddr)
#define LIMINAL_VK_DEVICE_FUNCS(X) \
    X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateBuffer) X(vkDestroyBuffer) \
    X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) X(vkBindBufferMemory) \
    X(vkMapMemory) X(vkUnmapMemory) X(vkCreateShaderModule) X(vkDestroyShaderModule) \
    X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreatePipelineLayout) \
    X(vkDestroyPipelineLayout) X(vkCreateComputePipelines) X(vkDestroyPipeline) X(vkCreateCommandPool) \
    X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
    X(vkResetCommandBuffer) X(vkCmdBindPipeline) X(vkCmdPushConstants) X(vkCmdDispatch) \
    X(vkCmdPipelineBarrier) X(vkCmdCopyBuffer) X(vkQueueSubmit) X(vkCreateFence) X(vkDestroyFence) \
    X(vkWaitForFences) X(vkResetFences) X(vkDeviceWaitIdle) X(vkCmdPushDescriptorSetKHR)

struct Api {
    void* library = nullptr;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
#define LIMINAL_VK_DECLARE(name) PFN_##name name = nullptr;
    LIMINAL_VK_INSTANCE_FUNCS(LIMINAL_VK_DECLARE)
    LIMINAL_VK_DEVICE_FUNCS(LIMINAL_VK_DECLARE)
#undef LIMINAL_VK_DECLARE
};

static void* OpenLoader()
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryA("vulkan-1.dll"));
#else
#if defined(__APPLE__)
    const char* names[] = {"libvulkan.1.dylib", "libvulkan.dylib", "/opt/homebrew/lib/libvulkan.1.dylib",
        "/usr/local/lib/libvulkan.1.dylib"};
#else
    const char* names[] = {"libvulkan.so.1", "libvulkan.so"};
#endif
    for (const char* name : names)
        if (void* library = dlopen(name, RTLD_NOW | RTLD_LOCAL)) return library;
    return nullptr;
#endif
}

static void* LoaderSymbol(void* library, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(library), name));
#else
    return dlsym(library, name);
#endif
}

static void CloseLoader(void* library)
{
    if (!library) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

// A tensor placed inside one of the weight buffers.
struct Tensor {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
};

struct QWeight {
    Tensor tensor;
    bool q6 = false;
    uint32_t rows = 0;
    uint32_t cols = 0;
    uint32_t row_bytes = 0;
};

struct Layer {
    bool swa = false;
    uint32_t head_dim = 0;
    bool owns_kv = false;
    int kv_source = 0;
    float output_scale = 1.0f;
    Tensor attn_norm, q_norm, k_norm, post_attention_norm, ffn_norm, post_ffw_norm, post_norm, inv_freq;
    QWeight wq, wk, wv, wo, gate, up, down, inp_gate, proj;
};

enum Kernel {
    kGemvQ4k, kGemvQ6k, kGemvF16, kDequantQ4k, kDequantQ6k, kGemm, kRmsNorm, kAddNorm, kAddNormNext,
    kQkvPrep256, kQkvPrep512, kAttnSwa, kAttnFull, kReduce256, kReduce512, kSoftmaxMask, kGeglu, kPlePrep,
    kKernelCount
};

constexpr int kMaxChunk = 512;      // tokens per recorded forward
constexpr uint32_t kAttnSplit = 64;  // keys per decode-attention workgroup
constexpr uint32_t kBindings = 9;

}  // namespace

struct VulkanModel {
    Api vk;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    VkPhysicalDeviceMemoryProperties memory{};
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipelines[kKernelCount] = {};
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::string device_name;

    const E4bModel* source = nullptr;
    std::vector<Buffer> weight_buffers;
    Tensor token_embd, proj_f16, proj_norm, output_norm;
    Layer layers[kE4bLayers];

    // Activations for up to kMaxChunk tokens (device-local) and host-visible I/O.
    Buffer x, h, y, qr, kr, vr, qo, o, gate, up, inp, pp, ple, scratch16, input, logits;
};

struct VulkanState {
    VulkanModel* model = nullptr;
    int capacity = 0;
    int n_past = 0;
    Buffer k[kE4bKvLayers], v[kE4bKvLayers];
    Buffer scores, partial_o, partial_ml;
    uint32_t max_splits = 0;
};

namespace {

#define LIMINAL_VK_CHECK(call, what) \
    do { const VkResult result_ = (call); if (result_ != VK_SUCCESS) { *error = std::string(what) + " failed (VkResult " + std::to_string(result_) + ")"; return false; } } while (0)

static bool FindMemoryType(const VulkanModel* m, uint32_t bits, VkMemoryPropertyFlags wanted, uint32_t* index)
{
    for (uint32_t i = 0; i < m->memory.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (m->memory.memoryTypes[i].propertyFlags & wanted) == wanted) { *index = i; return true; }
    return false;
}

static bool CreateBuffer(VulkanModel* m, VkDeviceSize size, bool host, Buffer* out, std::string* error)
{
    const Api& vk = m->vk;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = std::max<VkDeviceSize>(size, 16);
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    LIMINAL_VK_CHECK(vk.vkCreateBuffer(m->device, &info, nullptr, &out->buffer), "vkCreateBuffer");
    VkMemoryRequirements requirements;
    vk.vkGetBufferMemoryRequirements(m->device, out->buffer, &requirements);
    uint32_t type = 0;
    const bool found = host
        ? FindMemoryType(m, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &type) ||
          FindMemoryType(m, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &type)
        : FindMemoryType(m, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &type) ||
          FindMemoryType(m, requirements.memoryTypeBits, 0, &type);
    if (!found) { *error = "No suitable Vulkan memory type"; return false; }
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
    LIMINAL_VK_CHECK(vk.vkAllocateMemory(m->device, &allocate, nullptr, &out->memory), "vkAllocateMemory");
    LIMINAL_VK_CHECK(vk.vkBindBufferMemory(m->device, out->buffer, out->memory, 0), "vkBindBufferMemory");
    out->size = info.size;
    if (host) LIMINAL_VK_CHECK(vk.vkMapMemory(m->device, out->memory, 0, VK_WHOLE_SIZE, 0, &out->mapped), "vkMapMemory");
    return true;
}

static void DestroyBuffer(VulkanModel* m, Buffer* buffer)
{
    if (buffer->buffer) m->vk.vkDestroyBuffer(m->device, buffer->buffer, nullptr);
    if (buffer->memory) m->vk.vkFreeMemory(m->device, buffer->memory, nullptr);
    *buffer = Buffer();
}

static bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    for (const auto& extension : extensions) if (strcmp(extension.extensionName, name) == 0) return true;
    return false;
}

static bool CreateInstance(VulkanModel* m, std::string* error)
{
    Api& vk = m->vk;
    vk.library = OpenLoader();
    if (!vk.library) { *error = "Vulkan loader not found"; return false; }
    vk.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(LoaderSymbol(vk.library, "vkGetInstanceProcAddr"));
    if (!vk.vkGetInstanceProcAddr) { *error = "Vulkan loader has no vkGetInstanceProcAddr"; return false; }
    vk.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vk.vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    vk.vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
        vk.vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties"));
    if (!vk.vkCreateInstance || !vk.vkEnumerateInstanceExtensionProperties) { *error = "Incomplete Vulkan loader"; return false; }
    uint32_t count = 0;
    vk.vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vk.vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
    std::vector<const char*> extensions;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    // MoltenVK is a portability implementation and is only listed on request.
    if (HasExtension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Liminal";
    app.pEngineName = "liminal-e4b";
    app.apiVersion = VK_API_VERSION_1_2;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    LIMINAL_VK_CHECK(vk.vkCreateInstance(&info, nullptr, &m->instance), "vkCreateInstance");
#define LIMINAL_VK_LOAD(name) vk.name = reinterpret_cast<PFN_##name>(vk.vkGetInstanceProcAddr(m->instance, #name)); \
    if (!vk.name) { *error = "Vulkan entry point missing: " #name; return false; }
    LIMINAL_VK_INSTANCE_FUNCS(LIMINAL_VK_LOAD)
#undef LIMINAL_VK_LOAD
    return true;
}

// Picks a device with Vulkan 1.2, push descriptors, 8/16-bit storage and a
// compute queue: discrete first, then integrated, unless LIMINAL_VULKAN_DEVICE.
static bool PickDevice(VulkanModel* m, std::string* error)
{
    Api& vk = m->vk;
    uint32_t count = 0;
    vk.vkEnumeratePhysicalDevices(m->instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vk.vkEnumeratePhysicalDevices(m->instance, &count, devices.data());
    const char* forced = std::getenv("LIMINAL_VULKAN_DEVICE");
    int best_rank = -1;
    for (uint32_t i = 0; i < count; ++i) {
        if (forced && atoi(forced) != static_cast<int>(i)) continue;
        VkPhysicalDeviceProperties properties;
        vk.vkGetPhysicalDeviceProperties(devices[i], &properties);
        if (properties.apiVersion < VK_API_VERSION_1_2) continue;
        uint32_t extension_count = 0;
        vk.vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vk.vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extension_count, extensions.data());
        if (!HasExtension(extensions, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) continue;
        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        f11.pNext = &f12;
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &f11;
        vk.vkGetPhysicalDeviceFeatures2(devices[i], &features);
        if (!f11.storageBuffer16BitAccess || !f12.storageBuffer8BitAccess) continue;
        uint32_t family_count = 0;
        vk.vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vk.vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, families.data());
        int family = -1;
        for (uint32_t f = 0; f < family_count && family < 0; ++f)
            if (families[f].queueFlags & VK_QUEUE_COMPUTE_BIT) family = static_cast<int>(f);
        if (family < 0) continue;
        const int rank = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
            : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
            : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 0 : 1;
        if (rank > best_rank) {
            best_rank = rank;
            m->physical = devices[i];
            m->queue_family = static_cast<uint32_t>(family);
            m->device_name = std::string("vulkan:") + properties.deviceName;
        }
    }
    if (!m->physical) { *error = "No Vulkan 1.2 GPU with push descriptors and 8/16-bit storage"; return false; }
    vk.vkGetPhysicalDeviceMemoryProperties(m->physical, &m->memory);

    uint32_t extension_count = 0;
    vk.vkEnumerateDeviceExtensionProperties(m->physical, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available(extension_count);
    vk.vkEnumerateDeviceExtensionProperties(m->physical, nullptr, &extension_count, available.data());
    std::vector<const char*> extensions = {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    if (HasExtension(available, "VK_KHR_portability_subset")) extensions.push_back("VK_KHR_portability_subset");
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.storageBuffer8BitAccess = VK_TRUE;
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f11.storageBuffer16BitAccess = VK_TRUE;
    f11.pNext = &f12;
    VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vk.vkGetPhysicalDeviceFeatures2(m->physical, &supported);
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.features.shaderInt16 = supported.features.shaderInt16;
    features.pNext = &f11;
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = m->queue_family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.pNext = &features;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    LIMINAL_VK_CHECK(vk.vkCreateDevice(m->physical, &info, nullptr, &m->device), "vkCreateDevice");
#define LIMINAL_VK_LOAD(name) vk.name = reinterpret_cast<PFN_##name>(vk.vkGetDeviceProcAddr(m->device, #name)); \
    if (!vk.name) { *error = "Vulkan entry point missing: " #name; return false; }
    LIMINAL_VK_DEVICE_FUNCS(LIMINAL_VK_LOAD)
#undef LIMINAL_VK_LOAD
    vk.vkGetDeviceQueue(m->device, m->queue_family, 0, &m->queue);
    return true;
}

struct KernelSource {
    const uint32_t* code;
    size_t bytes;
    std::vector<uint32_t> constants;  // specialization constants 0..n-1
};

static bool CreatePipelines(VulkanModel* m, std::string* error)
{
    const Api& vk = m->vk;
    VkDescriptorSetLayoutBinding bindings[kBindings];
    for (uint32_t i = 0; i < kBindings; ++i)
        bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    set_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    set_info.bindingCount = kBindings;
    set_info.pBindings = bindings;
    LIMINAL_VK_CHECK(vk.vkCreateDescriptorSetLayout(m->device, &set_info, nullptr, &m->set_layout), "vkCreateDescriptorSetLayout");
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, 128};
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &m->set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &range;
    LIMINAL_VK_CHECK(vk.vkCreatePipelineLayout(m->device, &layout_info, nullptr, &m->pipeline_layout), "vkCreatePipelineLayout");

    const KernelSource sources[kKernelCount] = {
        {kSpvGemvQ4k, sizeof(kSpvGemvQ4k), {}},
        {kSpvGemvQ6k, sizeof(kSpvGemvQ6k), {}},
        {kSpvGemvF16, sizeof(kSpvGemvF16), {}},
        {kSpvDequantQ4k, sizeof(kSpvDequantQ4k), {}},
        {kSpvDequantQ6k, sizeof(kSpvDequantQ6k), {}},
        {kSpvGemm, sizeof(kSpvGemm), {}},
        {kSpvRmsNorm, sizeof(kSpvRmsNorm), {}},
        {kSpvAddNorm, sizeof(kSpvAddNorm), {0}},
        {kSpvAddNorm, sizeof(kSpvAddNorm), {1}},
        {kSpvQkvPrep, sizeof(kSpvQkvPrep), {256}},
        {kSpvQkvPrep, sizeof(kSpvQkvPrep), {512}},
        {kSpvAttnDecode, sizeof(kSpvAttnDecode), {256, static_cast<uint32_t>(kE4bSlidingWindow)}},
        {kSpvAttnDecode, sizeof(kSpvAttnDecode), {512, 0}},
        {kSpvAttnReduce, sizeof(kSpvAttnReduce), {256}},
        {kSpvAttnReduce, sizeof(kSpvAttnReduce), {512}},
        {kSpvSoftmaxMask, sizeof(kSpvSoftmaxMask), {}},
        {kSpvGeglu, sizeof(kSpvGeglu), {}},
        {kSpvPlePrep, sizeof(kSpvPlePrep), {}},
    };
    for (int k = 0; k < kKernelCount; ++k) {
        VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module_info.codeSize = sources[k].bytes;
        module_info.pCode = sources[k].code;
        VkShaderModule module = VK_NULL_HANDLE;
        LIMINAL_VK_CHECK(vk.vkCreateShaderModule(m->device, &module_info, nullptr, &module), "vkCreateShaderModule");
        std::vector<VkSpecializationMapEntry> entries;
        for (uint32_t i = 0; i < sources[k].constants.size(); ++i) entries.push_back({i, i * 4, 4});
        VkSpecializationInfo specialization{static_cast<uint32_t>(entries.size()), entries.data(),
            sources[k].constants.size() * 4, sources[k].constants.data()};
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = module;
        info.stage.pName = "main";
        info.stage.pSpecializationInfo = entries.empty() ? nullptr : &specialization;
        info.layout = m->pipeline_layout;
        const VkResult result = vk.vkCreateComputePipelines(m->device, VK_NULL_HANDLE, 1, &info, nullptr, &m->pipelines[k]);
        vk.vkDestroyShaderModule(m->device, module, nullptr);
        if (result != VK_SUCCESS) { *error = "vkCreateComputePipelines failed for kernel " + std::to_string(k); return false; }
    }
    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m->queue_family;
    LIMINAL_VK_CHECK(vk.vkCreateCommandPool(m->device, &pool_info, nullptr, &m->pool), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_info.commandPool = m->pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = 1;
    LIMINAL_VK_CHECK(vk.vkAllocateCommandBuffers(m->device, &cmd_info, &m->cmd), "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    LIMINAL_VK_CHECK(vk.vkCreateFence(m->device, &fence_info, nullptr, &m->fence), "vkCreateFence");
    return true;
}

static bool Begin(VulkanModel* m, std::string* error)
{
    LIMINAL_VK_CHECK(m->vk.vkResetCommandBuffer(m->cmd, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    LIMINAL_VK_CHECK(m->vk.vkBeginCommandBuffer(m->cmd, &info), "vkBeginCommandBuffer");
    return true;
}

static bool SubmitAndWait(VulkanModel* m, std::string* error)
{
    const Api& vk = m->vk;
    LIMINAL_VK_CHECK(vk.vkEndCommandBuffer(m->cmd), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m->cmd;
    LIMINAL_VK_CHECK(vk.vkResetFences(m->device, 1, &m->fence), "vkResetFences");
    LIMINAL_VK_CHECK(vk.vkQueueSubmit(m->queue, 1, &submit, m->fence), "vkQueueSubmit");
    LIMINAL_VK_CHECK(vk.vkWaitForFences(m->device, 1, &m->fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    return true;
}

// Compute-to-compute (and transfer) hazard barrier between dependent dispatches.
static void Barrier(VulkanModel* m)
{
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    m->vk.vkCmdPipelineBarrier(m->cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

struct Bind {
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize range;
};

static Bind B(const Buffer& buffer) { return {buffer.buffer, 0, VK_WHOLE_SIZE}; }
static Bind B(const Tensor& tensor) { return {tensor.buffer, tensor.offset, tensor.size}; }

template <typename Push>
static void Dispatch(VulkanModel* m, Kernel kernel, std::initializer_list<Bind> binds, const Push& push,
    uint32_t gx, uint32_t gy = 1, uint32_t gz = 1)
{
    const Api& vk = m->vk;
    VkDescriptorBufferInfo infos[kBindings];
    VkWriteDescriptorSet writes[kBindings];
    uint32_t count = 0;
    for (const Bind& bind : binds) {
        infos[count] = {bind.buffer, bind.offset, bind.range};
        writes[count] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[count].dstBinding = count;
        writes[count].descriptorCount = 1;
        writes[count].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[count].pBufferInfo = &infos[count];
        ++count;
    }
    vk.vkCmdBindPipeline(m->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m->pipelines[kernel]);
    vk.vkCmdPushDescriptorSetKHR(m->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m->pipeline_layout, 0, count, writes);
    vk.vkCmdPushConstants(m->cmd, m->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);
    vk.vkCmdDispatch(m->cmd, gx, gy, gz);
}

// Splits a flat workgroup count over x/y (kernels index with flat_group()).
static void Grid(uint32_t groups, uint32_t* gx, uint32_t* gy)
{
    *gx = std::min<uint32_t>(groups, 32768);
    *gy = (groups + *gx - 1) / *gx;
}

// ---- Weight upload --------------------------------------------------------

struct Uploader {
    VulkanModel* m;
    Buffer staging;
    size_t chunk_used = 0;  // bytes used in the current weight buffer
    static constexpr VkDeviceSize kChunk = 1ull << 30;
    static constexpr VkDeviceSize kStaging = 64ull << 20;

    bool Place(VkDeviceSize size, Tensor* out, std::string* error)
    {
        size = (size + 255) & ~VkDeviceSize(255);
        if (m->weight_buffers.empty() || chunk_used + size > m->weight_buffers.back().size) {
            Buffer buffer;
            if (!CreateBuffer(m, std::max(kChunk, size), false, &buffer, error)) return false;
            m->weight_buffers.push_back(buffer);
            chunk_used = 0;
        }
        out->buffer = m->weight_buffers.back().buffer;
        out->offset = chunk_used;
        out->size = size;
        chunk_used += size;
        return true;
    }

    bool Upload(const void* data, VkDeviceSize size, Tensor* out, std::string* error)
    {
        if (!Place(size, out, error)) return false;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (VkDeviceSize done = 0; done < size; done += kStaging) {
            const VkDeviceSize part = std::min(kStaging, size - done);
            memcpy(staging.mapped, bytes + done, part);
            if (!Begin(m, error)) return false;
            const VkBufferCopy copy{0, out->offset + done, part};
            m->vk.vkCmdCopyBuffer(m->cmd, staging.buffer, out->buffer, 1, &copy);
            if (!SubmitAndWait(m, error)) return false;
        }
        return true;
    }

    bool Quant(const WeightRef& w, QWeight* out, std::string* error)
    {
        if (w.type != kGgmlQ4K && w.type != kGgmlQ6K) { *error = "Vulkan backend expects Q4_K/Q6_K weights"; return false; }
        if (w.rows % 4 != 0) { *error = "GEMV needs a multiple of 4 rows"; return false; }
        out->q6 = w.type == kGgmlQ6K;
        out->rows = static_cast<uint32_t>(w.rows);
        out->cols = static_cast<uint32_t>(w.cols);
        out->row_bytes = static_cast<uint32_t>(w.row_bytes);
        return Upload(w.data, w.row_bytes * static_cast<size_t>(w.rows), &out->tensor, error);
    }

    bool F32(const WeightRef& w, Tensor* out, std::string* error)
    {
        return Upload(w.data, sizeof(float) * static_cast<size_t>(w.cols * w.rows), out, error);
    }
};

static uint16_t FloatToHalf(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, 4);
    const uint32_t sign = (bits >> 16) & 0x8000u;
    const int exponent = static_cast<int>((bits >> 23) & 0xff) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffffu;
    if (exponent <= 0) {
        if (exponent < -10) return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const int shift = 14 - exponent;
        uint32_t half = mantissa >> shift;
        if ((mantissa >> (shift - 1)) & 1u) ++half;  // round half up
        return static_cast<uint16_t>(sign | half);
    }
    if (exponent >= 31) return static_cast<uint16_t>(sign | 0x7c00u);
    uint32_t half = sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    if (mantissa & 0x1000u) ++half;  // round to nearest (ties up)
    return static_cast<uint16_t>(half);
}

static bool UploadWeights(VulkanModel* m, const E4bModel& model, std::string* error)
{
    Uploader up{m};
    if (!CreateBuffer(m, Uploader::kStaging, true, &up.staging, error)) return false;
    bool ok = true;
    ok = ok && up.Upload(model.token_embd.data, model.token_embd.row_bytes * static_cast<size_t>(model.token_embd.rows), &m->token_embd, error);
    {
        // per_layer_model_proj is BF16 [10752 rows x 2560]; the GPU reads f16.
        const WeightRef& proj = model.per_layer_model_proj;
        const size_t count = static_cast<size_t>(proj.rows * proj.cols);
        std::vector<uint16_t> half(count);
        const uint16_t* bf16 = reinterpret_cast<const uint16_t*>(proj.data);
        for (size_t i = 0; i < count; ++i) half[i] = FloatToHalf(Bf16ToFloat(bf16[i]));
        ok = ok && up.Upload(half.data(), count * 2, &m->proj_f16, error);
    }
    ok = ok && up.F32(model.per_layer_proj_norm, &m->proj_norm, error) && up.F32(model.output_norm, &m->output_norm, error);
    for (int l = 0; ok && l < kE4bLayers; ++l) {
        const E4bLayer& src = model.layers[l];
        Layer& dst = m->layers[l];
        dst.swa = src.swa;
        dst.head_dim = static_cast<uint32_t>(src.head_dim);
        dst.owns_kv = src.owns_kv;
        dst.kv_source = src.kv_source;
        dst.output_scale = src.output_scale;
        // Full layers scale each frequency by rope_freqs (64 rotating pairs out of 256).
        std::vector<float> inv_freq(static_cast<size_t>(src.head_dim / 2));
        for (size_t i = 0; i < inv_freq.size(); ++i) {
            const double exponent = 2.0 * static_cast<double>(i) / static_cast<double>(src.head_dim);
            inv_freq[i] = static_cast<float>(1.0 / (std::pow(static_cast<double>(src.rope_base), exponent) *
                (src.swa ? 1.0 : static_cast<double>(model.rope_freq_factors[i]))));
        }
        ok = up.Upload(inv_freq.data(), inv_freq.size() * 4, &dst.inv_freq, error) &&
            up.F32(src.attn_norm, &dst.attn_norm, error) && up.F32(src.q_norm, &dst.q_norm, error) &&
            up.F32(src.k_norm, &dst.k_norm, error) && up.F32(src.post_attention_norm, &dst.post_attention_norm, error) &&
            up.F32(src.ffn_norm, &dst.ffn_norm, error) && up.F32(src.post_ffw_norm, &dst.post_ffw_norm, error) &&
            up.F32(src.post_norm, &dst.post_norm, error) && up.Quant(src.wq, &dst.wq, error) &&
            (!src.owns_kv || (up.Quant(src.wk, &dst.wk, error) && up.Quant(src.wv, &dst.wv, error))) &&
            up.Quant(src.wo, &dst.wo, error) && up.Quant(src.w_gate, &dst.gate, error) && up.Quant(src.w_up, &dst.up, error) &&
            up.Quant(src.w_down, &dst.down, error) && up.Quant(src.inp_gate, &dst.inp_gate, error) &&
            up.Quant(src.proj, &dst.proj, error);
    }
    DestroyBuffer(m, &up.staging);
    return ok;
}

static bool CreateActivations(VulkanModel* m, std::string* error)
{
    const VkDeviceSize n = kMaxChunk;
    const VkDeviceSize f = sizeof(float);
    const VkDeviceSize ple = static_cast<VkDeviceSize>(kE4bLayers) * kE4bPleDim;
    const VkDeviceSize q_max = static_cast<VkDeviceSize>(kE4bHeads) * kE4bHeadDimFull;
    const VkDeviceSize kv_max = static_cast<VkDeviceSize>(kE4bKvHeads) * kE4bHeadDimFull;
    return CreateBuffer(m, n * kE4bEmbd * f, false, &m->x, error) &&
        CreateBuffer(m, n * kE4bEmbd * f, false, &m->h, error) &&
        CreateBuffer(m, n * kE4bEmbd * f, false, &m->y, error) &&
        CreateBuffer(m, n * q_max * f, false, &m->qr, error) &&
        CreateBuffer(m, n * kv_max * f, false, &m->kr, error) &&
        CreateBuffer(m, n * kv_max * f, false, &m->vr, error) &&
        CreateBuffer(m, n * q_max * f, false, &m->qo, error) &&
        CreateBuffer(m, n * q_max * f, false, &m->o, error) &&
        CreateBuffer(m, n * kE4bFfn * f, false, &m->gate, error) &&
        CreateBuffer(m, n * kE4bFfn * f, false, &m->up, error) &&
        CreateBuffer(m, n * kE4bPleDim * f, false, &m->inp, error) &&
        CreateBuffer(m, n * ple * f, false, &m->pp, error) &&
        CreateBuffer(m, n * ple * f, false, &m->ple, error) &&
        CreateBuffer(m, static_cast<VkDeviceSize>(kE4bFfn) * kE4bEmbd * 2, false, &m->scratch16, error) &&
        CreateBuffer(m, n * (kE4bEmbd + ple) * f, true, &m->input, error) &&
        CreateBuffer(m, static_cast<VkDeviceSize>(kE4bVocab) * f, true, &m->logits, error);
}

// ---- Forward pass -----------------------------------------------------------

struct GemvPush { uint32_t rows, row_bytes, x_off, o_off; float softcap; };
struct GemvF16Push { uint32_t rows, cols, x_off, o_off; float scale; };
struct DequantPush { uint32_t rows, row_bytes; };
struct GemmPush {
    uint32_t m, n, k;
    uint32_t a_off, a_sm, a_bs;
    uint32_t b_off, b_sn, b_sk, b_bs, b_div;
    uint32_t c_off, c_sm, c_bs;
    float alpha;
};

static void Gemm(VulkanModel* m, const Bind& a, const Bind& b, const Bind& c, const GemmPush& push, uint32_t batch)
{
    Dispatch(m, kGemm, {a, b, c}, push, (push.n + 63) / 64, (push.m + 63) / 64, batch);
}

// out[n, rows] = in[n, cols] . W^T. Decode uses the K-quant GEMV; prefill
// expands W to f16 in scratch16 and runs the GEMM. Ends with a barrier in
// prefill (scratch16 is reused); decode callers place their own.
static void Linear(VulkanModel* m, const QWeight& w, const Buffer& in, const Buffer& out, uint32_t n)
{
    if (n == 1) {
        uint32_t gx, gy;
        Grid(w.rows / 4, &gx, &gy);
        Dispatch(m, w.q6 ? kGemvQ6k : kGemvQ4k, {B(w.tensor), B(in), B(out)}, GemvPush{w.rows, w.row_bytes, 0, 0, 0.0f}, gx, gy);
        return;
    }
    uint32_t gx, gy;
    Grid((w.rows * (w.cols / 32) + 255) / 256, &gx, &gy);
    Dispatch(m, w.q6 ? kDequantQ6k : kDequantQ4k, {B(w.tensor), B(m->scratch16)}, DequantPush{w.rows, w.row_bytes}, gx, gy);
    Barrier(m);
    Gemm(m, B(in), B(m->scratch16), B(out), {n, w.rows, w.cols, 0, w.cols, 0, 0, w.cols, 1, 0, 1, 0, w.rows, 0, 1.0f}, 1);
    Barrier(m);
}

static bool EnsureScratch(VulkanState* s, std::string* error)
{
    if (s->scores.buffer) return true;
    VulkanModel* m = s->model;
    s->max_splits = static_cast<uint32_t>(s->capacity) / kAttnSplit + 2;
    return CreateBuffer(m, static_cast<VkDeviceSize>(kE4bHeads) * kMaxChunk * s->capacity * 4, false, &s->scores, error) &&
        CreateBuffer(m, static_cast<VkDeviceSize>(kE4bHeads) * s->max_splits * kE4bHeadDimFull * 4, false, &s->partial_o, error) &&
        CreateBuffer(m, static_cast<VkDeviceSize>(kE4bHeads) * s->max_splits * 2 * 4, false, &s->partial_ml, error);
}

static bool ForwardChunk(VulkanModel* m, VulkanState* s, const int32_t* tokens, uint32_t n, bool want_logits, std::string* error)
{
    const E4bModel& model = *m->source;
    const uint32_t E = kE4bEmbd;
    const uint32_t P = kE4bLayers * kE4bPleDim;
    const uint32_t p0 = static_cast<uint32_t>(s->n_past);
    const uint32_t T = p0 + n;
    const uint32_t cap = static_cast<uint32_t>(s->capacity);

    // Token and per-layer token embeddings are row gathers: done on the CPU from
    // the mapped GGUF, straight into the host-visible input buffer.
    float* x0 = static_cast<float*>(m->input.mapped);
    float* pt = x0 + static_cast<size_t>(kMaxChunk) * E;
    const float x_scale = std::sqrt(static_cast<float>(E));
    const float pt_scale = std::sqrt(static_cast<float>(kE4bPleDim));
    for (uint32_t t = 0; t < n; ++t) {
        float* xr = x0 + static_cast<size_t>(t) * E;
        float* pr = pt + static_cast<size_t>(t) * P;
        DequantizeWeightRow(model.token_embd, tokens[t], xr);
        DequantizeWeightRow(model.per_layer_token_embd, tokens[t], pr);
        for (uint32_t i = 0; i < E; ++i) xr[i] *= x_scale;
        for (uint32_t i = 0; i < P; ++i) pr[i] *= pt_scale;
    }

    if (!Begin(m, error)) return false;
    const VkBufferCopy copy{0, 0, static_cast<VkDeviceSize>(n) * E * 4};
    m->vk.vkCmdCopyBuffer(m->cmd, m->input.buffer, m->x.buffer, 1, &copy);
    Barrier(m);
    const float inv_sqrt_e = 1.0f / std::sqrt(static_cast<float>(E));
    if (n == 1) {
        uint32_t gx, gy;
        Grid(P / 4, &gx, &gy);
        Dispatch(m, kGemvF16, {B(m->proj_f16), B(m->x), B(m->pp)}, GemvF16Push{P, E, 0, 0, 1.0f}, gx, gy);
    } else {
        Gemm(m, B(m->x), B(m->proj_f16), B(m->pp), {n, P, E, 0, E, 0, 0, E, 1, 0, 1, 0, P, 0, 1.0f}, 1);
    }
    Dispatch(m, kRmsNorm, {B(m->x), B(m->layers[0].attn_norm), B(m->h)}, E, n);
    Barrier(m);
    struct PlePush { uint32_t pt_off; float pp_scale; };
    Dispatch(m, kPlePrep, {B(m->pp), B(m->input), B(m->proj_norm), B(m->ple)}, PlePush{kMaxChunk * E, inv_sqrt_e}, kE4bLayers, n);
    Barrier(m);

    struct PrepPush { uint32_t n, p0, cap; };
    struct AttnPush { uint32_t pos, cap, first_split, n_splits; };
    struct SoftmaxPush { uint32_t n, len, p0, key_start, window; };
    struct GegluPush { uint32_t rows, cols, b_off, b_stride; };
    for (int l = 0; l < kE4bLayers; ++l) {
        const Layer& L = m->layers[l];
        const uint32_t hd = L.head_dim;
        const Buffer& kc = s->k[L.kv_source];
        const Buffer& vc = s->v[L.kv_source];
        Linear(m, L.wq, m->h, m->qr, n);
        if (L.owns_kv) {
            Linear(m, L.wk, m->h, m->kr, n);
            Linear(m, L.wv, m->h, m->vr, n);
        }
        Barrier(m);
        const Kernel prep = hd == 512 ? kQkvPrep512 : kQkvPrep256;
        if (L.owns_kv) {
            Dispatch(m, prep, {B(m->qr), B(m->kr), B(m->vr), B(L.q_norm), B(L.k_norm), B(L.inv_freq), B(m->qo), B(s->k[l]), B(s->v[l])},
                PrepPush{n, p0, cap}, kE4bHeads + 2 * kE4bKvHeads, n);
        } else {
            // Query slots only; the K/V bindings are never written.
            Dispatch(m, prep, {B(m->qr), B(m->qr), B(m->qr), B(L.q_norm), B(L.q_norm), B(L.inv_freq), B(m->qo), B(kc), B(vc)},
                PrepPush{n, p0, cap}, kE4bHeads, n);
        }
        Barrier(m);
        if (n == 1) {
            const uint32_t first_key = L.swa ? (p0 + 1 > kE4bSlidingWindow ? p0 + 1 - kE4bSlidingWindow : 0) : 0;
            const uint32_t first_split = first_key / kAttnSplit;
            const uint32_t splits = p0 / kAttnSplit - first_split + 1;
            Dispatch(m, L.swa ? kAttnSwa : kAttnFull, {B(m->qo), B(kc), B(vc), B(s->partial_o), B(s->partial_ml)},
                AttnPush{p0, cap, first_split, splits}, splits, kE4bKvHeads);
            Barrier(m);
            Dispatch(m, hd == 512 ? kReduce512 : kReduce256, {B(s->partial_o), B(s->partial_ml), B(m->o)}, splits, kE4bHeads);
        } else {
            const uint32_t key_start = L.swa ? (p0 + 1 > kE4bSlidingWindow ? p0 + 1 - kE4bSlidingWindow : 0) : 0;
            const uint32_t len = T - key_start;
            Gemm(m, B(m->qo), B(kc), B(s->scores),
                {n, len, hd, 0, hd, n * hd, key_start * hd, hd, 1, cap * hd, 4, 0, len, n * len, 1.0f}, kE4bHeads);
            Barrier(m);
            Dispatch(m, kSoftmaxMask, {B(s->scores)}, SoftmaxPush{n, len, p0, key_start, L.swa ? uint32_t(kE4bSlidingWindow) : 0u},
                kE4bHeads * n);
            Barrier(m);
            Gemm(m, B(s->scores), B(vc), B(m->o),
                {n, hd, len, 0, len, n * len, key_start * hd, 1, hd, cap * hd, 4, 0, kE4bHeads * hd, hd, 1.0f}, kE4bHeads);
        }
        Barrier(m);
        Linear(m, L.wo, m->o, m->y, n);
        Barrier(m);
        Dispatch(m, kAddNormNext, {B(m->x), B(m->y), B(L.post_attention_norm), B(L.ffn_norm), B(m->h)}, 1.0f, n);
        Barrier(m);
        Linear(m, L.gate, m->h, m->gate, n);
        Linear(m, L.up, m->h, m->up, n);
        Barrier(m);
        {
            uint32_t gx, gy;
            Grid((n * kE4bFfn + 255) / 256, &gx, &gy);
            Dispatch(m, kGeglu, {B(m->gate), B(m->up)}, GegluPush{n, kE4bFfn, 0, kE4bFfn}, gx, gy);
        }
        Barrier(m);
        Linear(m, L.down, m->gate, m->y, n);
        Barrier(m);
        Dispatch(m, kAddNorm, {B(m->x), B(m->y), B(L.post_ffw_norm), B(L.post_ffw_norm), B(m->h)}, 1.0f, n);
        Barrier(m);
        Linear(m, L.inp_gate, m->x, m->inp, n);
        Barrier(m);
        {
            uint32_t gx, gy;
            Grid((n * kE4bPleDim + 255) / 256, &gx, &gy);
            Dispatch(m, kGeglu, {B(m->inp), B(m->ple)}, GegluPush{n, kE4bPleDim, static_cast<uint32_t>(l) * kE4bPleDim, P}, gx, gy);
        }
        Barrier(m);
        Linear(m, L.proj, m->inp, m->y, n);
        Barrier(m);
        const Tensor& next_norm = l + 1 < kE4bLayers ? m->layers[l + 1].attn_norm : m->output_norm;
        Dispatch(m, kAddNormNext, {B(m->x), B(m->y), B(L.post_norm), B(next_norm), B(m->h)}, L.output_scale, n);
        Barrier(m);
        // Prefill is submitted 7 layers at a time so a slow GPU stays far from
        // the Windows watchdog (TDR, ~2 s per submission).
        if (n > 1 && l % 7 == 6 && l + 1 < kE4bLayers && (!SubmitAndWait(m, error) || !Begin(m, error))) return false;
    }
    if (want_logits) {
        // After the last layer h is rms(x) * output_norm; the tied head reads token_embd.
        uint32_t gx, gy;
        Grid(kE4bVocab / 4, &gx, &gy);
        Dispatch(m, kGemvQ6k, {B(m->token_embd), B(m->h), B(m->logits)},
            GemvPush{kE4bVocab, static_cast<uint32_t>(model.token_embd.row_bytes), (n - 1) * E, 0, kE4bLogitSoftcap}, gx, gy);
        Barrier(m);
    }
    if (!SubmitAndWait(m, error)) return false;
    s->n_past = static_cast<int>(T);
    return true;
}

static void Destroy(VulkanModel* m)
{
    if (!m) return;
    const Api& vk = m->vk;
    if (m->device) {
        vk.vkDeviceWaitIdle(m->device);
        for (Buffer* b : {&m->x, &m->h, &m->y, &m->qr, &m->kr, &m->vr, &m->qo, &m->o, &m->gate, &m->up, &m->inp, &m->pp,
                 &m->ple, &m->scratch16, &m->input, &m->logits})
            DestroyBuffer(m, b);
        for (Buffer& b : m->weight_buffers) DestroyBuffer(m, &b);
        for (VkPipeline pipeline : m->pipelines) if (pipeline) vk.vkDestroyPipeline(m->device, pipeline, nullptr);
        if (m->pipeline_layout) vk.vkDestroyPipelineLayout(m->device, m->pipeline_layout, nullptr);
        if (m->set_layout) vk.vkDestroyDescriptorSetLayout(m->device, m->set_layout, nullptr);
        if (m->fence) vk.vkDestroyFence(m->device, m->fence, nullptr);
        if (m->pool) vk.vkDestroyCommandPool(m->device, m->pool, nullptr);
        vk.vkDestroyDevice(m->device, nullptr);
    }
    if (m->instance && vk.vkDestroyInstance) vk.vkDestroyInstance(m->instance, nullptr);
    CloseLoader(vk.library);
    delete m;
}

}  // namespace

VulkanModel* VulkanLoad(const E4bModel& model, std::string* error)
{
    std::unique_ptr<VulkanModel, void (*)(VulkanModel*)> m(new VulkanModel(), Destroy);
    m->source = &model;
    if (!CreateInstance(m.get(), error) || !PickDevice(m.get(), error) || !CreatePipelines(m.get(), error) ||
        !UploadWeights(m.get(), model, error) || !CreateActivations(m.get(), error)) {
        *error = "Vulkan: " + *error;
        return nullptr;
    }
    return m.release();
}

void VulkanFree(VulkanModel* model) { Destroy(model); }

const char* VulkanDeviceName(const VulkanModel* model) { return model->device_name.c_str(); }

VulkanState* VulkanStateCreate(VulkanModel* m, int capacity, std::string* error)
{
    std::unique_ptr<VulkanState> state(new VulkanState());
    state->model = m;
    state->capacity = capacity;
    for (int l = 0; l < kE4bKvLayers; ++l) {
        const VkDeviceSize bytes = static_cast<VkDeviceSize>(kE4bKvHeads) * capacity * m->layers[l].head_dim * 2;
        if (!CreateBuffer(m, bytes, false, &state->k[l], error) || !CreateBuffer(m, bytes, false, &state->v[l], error)) {
            VulkanStateFree(state.release());
            return nullptr;
        }
    }
    return state.release();
}

void VulkanStateFree(VulkanState* state)
{
    if (!state) return;
    VulkanModel* m = state->model;
    m->vk.vkDeviceWaitIdle(m->device);
    for (int l = 0; l < kE4bKvLayers; ++l) { DestroyBuffer(m, &state->k[l]); DestroyBuffer(m, &state->v[l]); }
    DestroyBuffer(m, &state->scores);
    DestroyBuffer(m, &state->partial_o);
    DestroyBuffer(m, &state->partial_ml);
    delete state;
}

bool VulkanForward(VulkanModel* m, VulkanState* s, const int32_t* tokens, int n, std::vector<float>* logits, std::string* error)
{
    if (s->n_past + n > s->capacity) { *error = "Context exhausted"; return false; }
    if (!EnsureScratch(s, error)) return false;
    for (int done = 0; done < n;) {
        const int count = std::min(kMaxChunk, n - done);
        if (!ForwardChunk(m, s, tokens + done, static_cast<uint32_t>(count), done + count == n, error)) {
            *error = "Vulkan forward: " + *error;
            return false;
        }
        done += count;
    }
    logits->resize(kE4bVocab);
    memcpy(logits->data(), m->logits.mapped, sizeof(float) * kE4bVocab);
    return true;
}

}  // namespace liminal::engine
