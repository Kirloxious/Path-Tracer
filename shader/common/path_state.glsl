#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

#include "host_shared.glsl"
#include "geom.glsl"

const uint FLAG_FRONT_FACE         = 1u << 0; // last hit's face was front-facing
const uint FLAG_PREV_NON_SPECULAR  = 1u << 1; // previous vertex ran area-light NEE, usable sample or not — an emissive hit MIS-weights against it
const uint FLAG_RESTIR_HANDLED     = 1u << 2; // previous vertex's reservoir was its whole area-light estimator, empty included — an emissive hit contributes nothing
const uint FLAG_SPECULAR_PREFIX    = 1u << 3; // every vertex so far was a perfect mirror, so this path still follows restir_initial's walk
const uint FLAG_PREV_ENV_NEE       = 1u << 4; // previous vertex ran environment NEE, usable sample or not — an escaping ray MIS-weights against it

// 96 bytes. Every `vec3 + scalar` pair fits in one 16-byte std430 slot.
struct PathState {
    vec3  throughput;
    uint  flags;
    vec3  radiance;
    uint  rng_state;           // sampler seed, per pixel and deliberately constant across frames:
                               // the frame is the sample index, so varying this makes it noise
    vec3  ray_origin;
    float pdf_bsdf;            // solid-angle pdf of the continuation direction; 0 after a delta lobe
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
