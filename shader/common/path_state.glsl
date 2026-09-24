#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

#include "host_shared.glsl"

// Bits in PathState.flags
const uint FLAG_FRONT_FACE         = 1u << 0; // last hit's face was front-facing
const uint FLAG_PREV_NON_SPECULAR  = 1u << 1; // previous bounce ran analytic light NEE (whether or not its sample was usable) — shade_emissive applies balance-heuristic MIS using s.pdf_bsdf and the analytic light pdf
const uint FLAG_RESTIR_HANDLED     = 1u << 2; // previous bounce owned a ReSTIR reservoir — shade_emissive skips its contribution, since the reservoir already estimated that vertex's area-light integral, empty reservoirs included
const uint FLAG_SPECULAR_PREFIX    = 1u << 3; // every vertex so far has been a perfect mirror, so this path is still tracking the chain restir_initial walked — the next diffuse vertex it reaches is that pixel's ReSTIR resampling surface
const uint FLAG_PREV_ENV_NEE       = 1u << 4; // previous bounce ran environment NEE (whether or not its sample was usable) — trace.comp MIS-weights an escaping ray against that density. Independent of the two flags above, which describe the area-light estimator

// hit_triangle_idx of a gbuffer-fed primary hit, which carries only the shading normal.
const uint NO_TRIANGLE = 0xFFFFFFFFu;

// 96 bytes. Every `vec3 + scalar` pair fits in one 16-byte std430 slot.
// NEE fields (nee_dir/nee_dist/nee_le) intentionally live in ShadowState below —
// only shade_surface writes them, only trace_shadow reads them, so keeping
// them out of the hot state cuts the per-thread traffic in every other kernel.
struct PathState {
    vec3  throughput;
    uint  flags;
    vec3  radiance;
    uint  rng_state;           // sampler seed: per-pixel, fixed for the whole run. Constant
                               // across frames on purpose — the frame is the sample index,
                               // and folding it in here would flatten the sequence to noise.
    vec3  ray_origin;
    float pdf_bsdf;            // pdf of the continuation-direction sample at the previous non-specular bounce; consumed by shade_emissive's MIS weight
    vec3  ray_dir;
    uint  bounce;
    vec3  hit_point;
    uint  hit_matid;
    vec3  hit_normal;
    uint  hit_triangle_idx;    // set by trace.comp, NO_TRIANGLE at the primary; shade_emissive's MIS and the ray-origin offsets read it
};

layout(std430, binding = BIND_PATH_STATE) restrict buffer PathStateBuffer {
    PathState states[];
};

// ShadowState lives in shadow_state.glsl — only the shade kernels and trace_shadow
// need it, so keeping it out of this header keeps other kernels under the
// NVIDIA GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS=16 cap.

#endif
