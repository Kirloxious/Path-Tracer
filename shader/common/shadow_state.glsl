#ifndef SHADOW_STATE_GLSL
#define SHADOW_STATE_GLSL

#include "host_shared.glsl"

// NEE plumbing from the shade kernels (writers) to trace_shadow (reader). Its own header, out
// of PathState, so only those kernels declare the SSBO — NVIDIA caps a compute shader at 16.
//
// Two independent slots, area light and environment: each technique's directions end where the
// other's cannot, so both can fire at one vertex without double counting.
struct ShadowState {
    vec3  nee_dir;
    float nee_dist;
    vec3  nee_le;
    float nee_valid;
    vec3  env_dir;
    float env_valid;
    vec3  env_le;
    uint  nee_tri; // sampled light triangle, exempt from its own shadow test
};

layout(std430, binding = BIND_SHADOW_STATE) restrict buffer ShadowStateBuffer {
    ShadowState shadow_states[];
};

#endif
