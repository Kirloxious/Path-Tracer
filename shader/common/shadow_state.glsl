#ifndef SHADOW_STATE_GLSL
#define SHADOW_STATE_GLSL

// NEE plumbing between shade_lambertian (writer) and trace_shadow (reader).
// Split out of PathState so kernels that don't touch NEE don't pay the traffic
// cost, and kept in its own header so only these two kernels declare the SSBO —
// leaves headroom under NVIDIA's GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS=16 cap.
// 32 bytes/pixel.
struct ShadowState {
    vec3  nee_dir;
    float nee_dist;
    vec3  nee_le;
    float _pad;
};

layout(std430, binding = 22) restrict buffer ShadowStateBuffer {
    ShadowState shadow_states[];
};

#endif
