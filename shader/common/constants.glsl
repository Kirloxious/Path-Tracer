#ifndef CONSTANTS_GLSL
#define CONSTANTS_GLSL

#include "host_shared.glsl"

// Mirrors FrameConstants in src/render/gpu_constants.h.
layout(std140, binding = UBO_FRAME) uniform FrameConstants {
    ivec2 image_size;
    // Counts from 1. Also the sampler's sample index, so it may only advance when a sample is accumulated.
    int   frame_index;
    // Unlike frame_index, survives camera motion.
    int   history_frames;
    uint  time_seed; // white-noise PCG streams
    uint  run_seed; // low-discrepancy sampler; constant per accumulation
};

// Mirrors SceneConstants in src/render/gpu_constants.h.
layout(std140, binding = UBO_SCENE) uniform SceneConstants {
    int   bvh_root_index;
    int   num_light_groups;
    ivec2 env_sample_size;
    int   max_bounces;
    float indirect_clamp;
    float env_map_intensity;
    int   env_map_valid;
};

#endif
