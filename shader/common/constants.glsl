#ifndef CONSTANTS_GLSL
#define CONSTANTS_GLSL

#include "host_shared.glsl"

// Mirrors FrameConstants in src/render/gpu_constants.h. Renderer::render() rewrites it before
// the first pass, so every kernel of a frame sees the same values.
layout(std140, binding = UBO_FRAME) uniform FrameConstants {
    ivec2 image_size;
    // Frames accumulated since the last reset — and the low-discrepancy sample index, so it
    // must advance only when a new sample is accumulated.
    int   frame_index;
    // Frames since temporal history (TAA, ReSTIR) was last invalidated. Survives camera motion.
    int   history_frames;
    uint  time_seed; // fresh every frame; seeds the white-noise PCG streams
    uint  run_seed;  // constant for one accumulation; seeds the low-discrepancy sampler
};

// Mirrors SceneConstants in src/render/gpu_constants.h. Written once by Renderer::loadScene().
layout(std140, binding = UBO_SCENE) uniform SceneConstants {
    int   bvh_root_index;
    // Emissive triangles are sorted to the front, so `tri_index > emissive_last_index` is an
    // exact "not a light" test. -1 when the scene has no emitters.
    int   emissive_last_index;
    int   num_light_groups;
    int   max_bounces;
    float indirect_clamp;
    float env_map_intensity;
    ivec2 env_sample_size;
    int   env_map_valid; // 0 = no envmap bound → sample_envmap returns black
};

#endif
