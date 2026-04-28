#ifndef QUEUE_GLSL
#define QUEUE_GLSL

// All queue counters share one SSBO. NVIDIA caps GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS
// at 16; with one counter per queue (×6) plus indices (×6) plus path_state (×1) plus
// scene (×5) we hit 18 and the link fails ("error C5058: no buffers available for
// bindable storage buffer"). Sharing the counter SSBO drops us to 13.

const uint Q_RAY        = 0u;
const uint Q_LAMB       = 1u;
const uint Q_METAL      = 2u;
const uint Q_DIELECTRIC = 3u;
const uint Q_EMISSIVE   = 4u;
const uint Q_SHADOW     = 5u;
const uint NUM_QUEUES   = 6u;

layout(std430, binding = 11) restrict buffer QueueCountersBuffer {
    uint q_count[NUM_QUEUES];
};

layout(std430, binding = 12) restrict buffer RayQueueIndices      { uint ray_queue_idx[]; };
layout(std430, binding = 13) restrict buffer HitLambertianIndices { uint hit_lambertian_idx[]; };
layout(std430, binding = 14) restrict buffer HitMetalIndices      { uint hit_metal_idx[]; };
layout(std430, binding = 15) restrict buffer HitDielectricIndices { uint hit_dielectric_idx[]; };
layout(std430, binding = 16) restrict buffer HitEmissiveIndices   { uint hit_emissive_idx[]; };
layout(std430, binding = 17) restrict buffer ShadowQueueIndices   { uint shadow_queue_idx[]; };

#define ray_queue_count      q_count[Q_RAY]
#define hit_lambertian_count q_count[Q_LAMB]
#define hit_metal_count      q_count[Q_METAL]
#define hit_dielectric_count q_count[Q_DIELECTRIC]
#define hit_emissive_count   q_count[Q_EMISSIVE]
#define shadow_queue_count   q_count[Q_SHADOW]

void ray_queue_push(uint pid)      { uint s = atomicAdd(q_count[Q_RAY],        1u); ray_queue_idx[s]      = pid; }
void hit_lambertian_push(uint pid) { uint s = atomicAdd(q_count[Q_LAMB],       1u); hit_lambertian_idx[s] = pid; }
void hit_metal_push(uint pid)      { uint s = atomicAdd(q_count[Q_METAL],      1u); hit_metal_idx[s]      = pid; }
void hit_dielectric_push(uint pid) { uint s = atomicAdd(q_count[Q_DIELECTRIC], 1u); hit_dielectric_idx[s] = pid; }
void hit_emissive_push(uint pid)   { uint s = atomicAdd(q_count[Q_EMISSIVE],   1u); hit_emissive_idx[s]   = pid; }
void shadow_queue_push(uint pid)   { uint s = atomicAdd(q_count[Q_SHADOW],     1u); shadow_queue_idx[s]   = pid; }

#endif
