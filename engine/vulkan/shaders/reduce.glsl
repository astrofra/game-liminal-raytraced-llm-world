// Sum over a 256-thread workgroup through `scratch` (256 floats).
float workgroup_sum(float value)
{
    const uint tid = gl_LocalInvocationID.x;
    scratch[tid] = value;
    barrier();
    [[unroll]] for (uint s = 128; s > 0; s >>= 1) {
        if (tid < s) scratch[tid] += scratch[tid + s];
        barrier();
    }
    const float total = scratch[0];
    barrier();
    return total;
}
