#ifndef QUEUE_GLSL
#define QUEUE_GLSL

// All queue counters share one SSBO: NVIDIA caps a compute shader at 16 storage blocks, and a
// counter block per queue pushed the worst kernel to 18 (link error C5058).

#include "host_shared.glsl"

layout(std430, binding = BIND_QUEUE_COUNTERS) restrict buffer QueueCountersBuffer {
    uint q_count[NUM_QUEUES];
};

layout(std430, binding = BIND_RAY_QUEUE) restrict buffer RayQueueIndices         { uint ray_queue_idx[]; };
layout(std430, binding = BIND_HIT_OPAQUE_QUEUE) restrict buffer HitOpaqueIndices        { uint hit_opaque_idx[]; };
layout(std430, binding = BIND_HIT_TRANSMISSIVE_QUEUE) restrict buffer HitTransmissiveIndices  { uint hit_transmissive_idx[]; };
layout(std430, binding = BIND_HIT_EMISSIVE_QUEUE) restrict buffer HitEmissiveIndices      { uint hit_emissive_idx[]; };
layout(std430, binding = BIND_SHADOW_QUEUE) restrict buffer ShadowQueueIndices      { uint shadow_queue_idx[]; };

// GL guarantees only 65535 workgroups per dispatch dimension, which a full-screen queue
// passes above ~4.2M pixels (4K is 8.3M), so prepare_indirect spills the count into Y.
const uint QUEUE_MAX_GROUPS_X = 65535u;

// Flattened thread index for a kernel dispatched over a queue by prepare_indirect.
uint queue_thread_index() {
    return gl_GlobalInvocationID.y * (gl_NumWorkGroups.x * gl_WorkGroupSize.x) + gl_GlobalInvocationID.x;
}

#define ray_queue_count        q_count[Q_RAY]
#define hit_opaque_count       q_count[Q_OPAQUE]
#define hit_transmissive_count q_count[Q_TRANSMISSIVE]
#define hit_emissive_count     q_count[Q_EMISSIVE]
#define shadow_queue_count     q_count[Q_SHADOW]

// Queue append, aggregated per subgroup: nearly every lane of a subgroup pushes to the same
// counter, so one atomicAdd per subgroup replaces up to 32. Safe inside divergent control flow —
// the ballot and readFirstInvocation both see exactly the lanes that are pushing.
#if defined(GL_ARB_shader_ballot) && defined(GL_ARB_gpu_shader_int64)

uint ballot_bit_count(uint64_t mask) {
    uvec2 halves = unpackUint2x32(mask);
    return uint(bitCount(halves.x) + bitCount(halves.y));
}

#define QUEUE_PUSH(slot, arr, pid)                                                                 \
    uint64_t _mask   = ballotARB(true);                                                            \
    uint     _n      = ballot_bit_count(_mask);                                                    \
    uint     _prefix = ballot_bit_count(_mask & gl_SubGroupLtMaskARB);                             \
    uint     _base   = 0u;                                                                         \
    if (_prefix == 0u) {                                                                           \
        _base = atomicAdd(q_count[slot], _n);                                                      \
    }                                                                                              \
    _base = readFirstInvocationARB(_base);                                                         \
    arr[_base + _prefix] = pid;

#else

#define QUEUE_PUSH(slot, arr, pid)                                                                 \
    uint _s = atomicAdd(q_count[slot], 1u);                                                        \
    arr[_s] = pid;

#endif

void ray_queue_push(uint pid)        { QUEUE_PUSH(Q_RAY,          ray_queue_idx,        pid) }
void hit_opaque_push(uint pid)       { QUEUE_PUSH(Q_OPAQUE,       hit_opaque_idx,       pid) }
void hit_transmissive_push(uint pid) { QUEUE_PUSH(Q_TRANSMISSIVE, hit_transmissive_idx, pid) }
void hit_emissive_push(uint pid)     { QUEUE_PUSH(Q_EMISSIVE,     hit_emissive_idx,     pid) }
void shadow_queue_push(uint pid)     { QUEUE_PUSH(Q_SHADOW,       shadow_queue_idx,     pid) }

#endif
