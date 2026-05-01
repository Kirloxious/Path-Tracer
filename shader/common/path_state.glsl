#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

// Bits in PathState.flags
const uint FLAG_FRONT_FACE         = 1u << 0; // last hit's face was front-facing
const uint FLAG_PREV_NON_SPECULAR  = 1u << 1; // previous bounce was non-specular — shade_emissive applies MIS weighting (specular/primary hits get weight 1)

struct PathState {
    vec3  throughput;
    uint  flags;
    vec3  radiance;
    uint  pixel_idx;
    uvec4 rng_state;
    vec3  ray_origin;
    float pdf_bsdf;            // pdf of the continuation-direction sample at the previous non-specular bounce; consumed by shade_emissive's MIS weight
    vec3  ray_dir;
    uint  bounce;
    vec3  hit_point;
    uint  hit_matid;
    vec3  hit_normal;
    float hit_t;
    vec3  nee_dir;
    float nee_dist;
    vec3  nee_le;
    uint  hit_triangle_idx;    // set by trace.comp, used by shade_emissive to look up the light-group pdf for MIS
};

layout(std430, binding = 10) restrict buffer PathStateBuffer {
    PathState states[];
};

#endif
