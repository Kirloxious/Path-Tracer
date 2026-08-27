#ifndef PATH_STATE_GLSL
#define PATH_STATE_GLSL

// Bits in PathState.flags
const uint FLAG_FRONT_FACE         = 1u << 0; // last hit's face was front-facing
const uint FLAG_PREV_NON_SPECULAR  = 1u << 1; // previous bounce ran analytic NEE — shade_emissive applies balance-heuristic MIS using s.pdf_bsdf and the analytic light pdf
const uint FLAG_RESTIR_HANDLED     = 1u << 2; // previous bounce was a ReSTIR-resolved primary with W>0 — shade_emissive skips its contribution to avoid double-counting the same direct-lighting estimator

// 96 bytes. Every `vec3 + scalar` pair fits in one 16-byte std430 slot.
// NEE fields (nee_dir/nee_dist/nee_le) intentionally live in ShadowState below —
// only shade_lambertian writes them, only trace_shadow reads them, so keeping
// them out of the hot state cuts the per-thread traffic in every other kernel.
struct PathState {
    vec3  throughput;
    uint  flags;
    vec3  radiance;
    uint  rng_state;           // was uvec4; only .x was ever read — 12 bytes recovered
    vec3  ray_origin;
    float pdf_bsdf;            // pdf of the continuation-direction sample at the previous non-specular bounce; consumed by shade_emissive's MIS weight
    vec3  ray_dir;
    uint  bounce;
    vec3  hit_point;
    uint  hit_matid;
    vec3  hit_normal;
    uint  hit_triangle_idx;    // set by trace.comp, used by shade_emissive to look up the light-group pdf for MIS
};

layout(std430, binding = 10) restrict buffer PathStateBuffer {
    PathState states[];
};

// ShadowState lives in shadow_state.glsl — only shade_lambertian and trace_shadow
// need it, so keeping it out of this header keeps other kernels under the
// NVIDIA GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS=16 cap.

#endif
