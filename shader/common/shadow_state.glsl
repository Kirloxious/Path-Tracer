#ifndef SHADOW_STATE_GLSL
#define SHADOW_STATE_GLSL

#include "host_shared.glsl"

// NEE plumbing between shade_surface (writer) and trace_shadow (reader).
// Split out of PathState so kernels that don't touch NEE don't pay the traffic
// cost, and kept in its own header so only these two kernels declare the SSBO —
// leaves headroom under NVIDIA's GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS=16 cap.
//
// Two independent slots, each with its own shadow ray. Area lights and the environment are
// disjoint estimators — a direction drawn toward an emissive triangle ends on it, one drawn
// toward the sky escapes, so neither can produce the other's samples — which is what lets both
// fire at the same vertex with no double counting. A scene with only emitters never fills the
// env slot and one lit only by an HDR never fills the light slot, so the second ray is paid
// for only where both kinds of light actually exist.
//
// 64 bytes/pixel.
struct ShadowState {
    vec3  nee_dir;
    float nee_dist;
    vec3  nee_le;
    float nee_valid;
    vec3  env_dir;
    float env_valid;
    vec3  env_le;
    float _pad;
};

layout(std430, binding = BIND_SHADOW_STATE) restrict buffer ShadowStateBuffer {
    ShadowState shadow_states[];
};

#endif
