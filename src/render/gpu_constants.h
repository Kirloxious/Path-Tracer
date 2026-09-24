#pragma once

/**
 * @file gpu_constants.h
 * @brief CPU mirrors of the frame- and scene-wide uniform blocks in shader/common/constants.glsl.
 */

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

/// Mirrors `FrameConstants` (std140). Rewritten by Renderer::render() before the first pass.
struct alignas(16) FrameConstants
{
    glm::ivec2 image_size{0};
    int32_t    frame_index = 0;
    int32_t    history_frames = 0;
    uint32_t   time_seed = 0;
    uint32_t   run_seed = 0;
    uint32_t   _pad[2] = {};
};
static_assert(sizeof(FrameConstants) == 32, "FrameConstants must match std140 layout");
static_assert(offsetof(FrameConstants, frame_index) == 8);
static_assert(offsetof(FrameConstants, time_seed) == 16);
static_assert(offsetof(FrameConstants, run_seed) == 20);

/// Mirrors `SceneConstants` (std140). Written by Renderer::loadScene().
struct alignas(16) SceneConstants
{
    int32_t    bvh_root_index = 0;
    int32_t    emissive_last_index = -1;
    int32_t    num_light_groups = 0;
    int32_t    max_bounces = 0;
    float      indirect_clamp = 0.0f;
    float      env_map_intensity = 1.0f;
    glm::ivec2 env_sample_size{0};
    int32_t    env_map_valid = 0;
    int32_t    _pad[3] = {};
};
static_assert(sizeof(SceneConstants) == 48, "SceneConstants must match std140 layout");
static_assert(offsetof(SceneConstants, indirect_clamp) == 16);
static_assert(offsetof(SceneConstants, env_sample_size) == 24);
static_assert(offsetof(SceneConstants, env_map_valid) == 32);
