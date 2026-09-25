#ifndef SHADOW_STATE_GLSL
#define SHADOW_STATE_GLSL

#include "host_shared.glsl"

// Separate from PathState so only its writers and trace_shadow declare it (16-block limit). The light and
// environment slots are independent: both can fire at one vertex without double counting.
struct ShadowState {
    vec3  nee_dir;
    float nee_dist;
    vec3  nee_le;
    float nee_valid;
    vec3  env_dir;
    float env_valid;
    vec3  env_le;
    uint  nee_tri; // exempt from its own shadow test
};

layout(std430, binding = BIND_SHADOW_STATE) restrict buffer ShadowStateBuffer {
    ShadowState shadow_states[];
};

#endif
