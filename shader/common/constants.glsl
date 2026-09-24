#ifndef CONSTANTS_GLSL
#define CONSTANTS_GLSL

#include "host_shared.glsl"

// Mirrors FrameConstants in src/render/gpu_constants.h.
layout(std140, binding = UBO_FRAME) uniform FrameConstants {
    ivec2 image_size;
    // Counts from 1. Also the low-discrepancy sample index (via sampler_init), so it must
    // advance only when a new sample is accumulated.
    int   frame_index;
    // Frames since temporal history (TAA, ReSTIR) was last invalidated. Survives camera motion.
    int   history_frames;
    uint  time_seed; // fresh every frame; seeds the white-noise PCG streams
    uint  run_seed;  // constant for one accumulation; seeds the low-discrepancy sampler
};

// Mirrors SceneConstants in src/render/gpu_constants.h.
layout(std140, binding = UBO_SCENE) uniform SceneConstants {
    int   bvh_root_index;
    // Emissive triangles are sorted to the front; -1 when the scene has no emitters.
    int   emissive_last_index;
    int   num_light_groups;
    int   max_bounces;
    float indirect_clamp;
    float env_map_intensity;
    ivec2 env_sample_size;
    int   env_map_valid; // 0 = no envmap bound → sample_envmap returns black
};

#endif
