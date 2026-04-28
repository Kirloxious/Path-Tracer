#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

// Bits in PathState.flags
const uint FLAG_FRONT_FACE   = 1u << 0;  // last hit's face was front-facing
const uint FLAG_PREV_DID_NEE = 1u << 1;  // previous bounce sampled a light via NEE — used to suppress double-counting in shade_emissive

struct PathState {
    vec3 throughput;
    uint flags;
    vec3 radiance;
    uint pixel_idx;
    uvec4 rng_state;
    vec3 ray_origin;
    float pdf_bsdf;
    vec3 ray_dir;
    uint bounce;
    vec3 hit_point;
    uint hit_matid;
    vec3 hit_normal;
    float hit_t;
    vec3 nee_dir;
    float nee_dist;
    vec3 nee_le;
    float _pad;
};

layout(std430, binding = 10) restrict buffer PathStateBuffer {
    PathState states[];
};

#endif
