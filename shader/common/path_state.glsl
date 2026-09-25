#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

#include "host_shared.glsl"
#include "geom.glsl"

const uint FLAG_FRONT_FACE         = 1u << 0;
const uint FLAG_PREV_NON_SPECULAR  = 1u << 1; // ran area-light NEE (usable sample or not)
const uint FLAG_RESTIR_HANDLED     = 1u << 2; // reservoir was the whole area-light estimator, empty included
const uint FLAG_SPECULAR_PREFIX    = 1u << 3; // all mirrors so far: still on restir_initial's walk
const uint FLAG_PREV_ENV_NEE       = 1u << 4; // ran environment NEE (usable sample or not)

// 96 bytes. Every `vec3 + scalar` pair fits in one 16-byte std430 slot.
struct PathState {
    vec3  throughput;
    uint  flags;
    vec3  radiance;
    uint  rng_state; // per pixel, constant across frames: the frame is the sample index
    vec3  ray_origin;
    float pdf_bsdf; // 0 after a delta lobe
    vec3  ray_dir;
    uint  bounce;
    vec3  hit_point;
    uint  hit_matid;
    vec3  hit_normal;
    uint  hit_triangle_idx;    // NO_TRIANGLE at a gbuffer primary
};

layout(std430, binding = BIND_PATH_STATE) restrict buffer PathStateBuffer {
    PathState states[];
};

#endif
