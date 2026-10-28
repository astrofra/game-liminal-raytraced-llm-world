// Shared helpers of the E4B Vulkan kernels. Weights are raw GGUF blocks read
// through 8/16-bit storage; activations are f32, the KV cache f16. No subgroup
// operation is used, so the kernels are independent of the device's subgroup
// size (8 to 64); reductions go through shared memory.
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_control_flow_attributes : enable

const float kNegInf = -1.0 / 0.0;

float gelu_tanh(float x)
{
    const float c = 0.7978845608028654;
    return 0.5 * x * (1.0 + tanh(c * (x + 0.044715 * x * x * x)));
}

// Workgroup id over a 2-D grid (the host splits counts above 65535).
uint flat_group()
{
    return gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
}
