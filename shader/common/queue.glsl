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

// Queue append.
//
// Every thread that reaches one of these pushes to the same counter address, so the naive
// form serializes one atomic per thread — and in trace.comp or generate.comp nearly every
// thread in a subgroup pushes to the *same* queue, so that is 32 atomics where one would
// do. Aggregating across the subgroup does a single atomicAdd for the whole group and hands
// each thread its slot from the ballot's prefix count.
//
// subgroupBallot() reflects the invocations active at the call site, which is exactly the
// set that is pushing — these are called from inside divergent control flow, and that is
// fine, since elect/broadcastFirst operate on the same active set.
#if defined(GL_ARB_shader_ballot) && defined(GL_ARB_gpu_shader_int64)

uint ballot_bit_count(uint64_t mask) {
    uvec2 halves = unpackUint2x32(mask);
    return uint(bitCount(halves.x) + bitCount(halves.y));
}

// One atomic for the whole subgroup: the lowest active lane reserves `count` slots and
// readFirstInvocationARB broadcasts the base back to everyone. Each lane's own slot is the
// base plus how many active lanes precede it.
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

// Fallback: one atomic per thread, as before.
#define QUEUE_PUSH(slot, arr, pid)                                                                 \
    uint _s = atomicAdd(q_count[slot], 1u);                                                        \
    arr[_s] = pid;

#endif

void ray_queue_push(uint pid)      { QUEUE_PUSH(Q_RAY,        ray_queue_idx,      pid) }
void hit_lambertian_push(uint pid) { QUEUE_PUSH(Q_LAMB,       hit_lambertian_idx, pid) }
void hit_metal_push(uint pid)      { QUEUE_PUSH(Q_METAL,      hit_metal_idx,      pid) }
void hit_dielectric_push(uint pid) { QUEUE_PUSH(Q_DIELECTRIC, hit_dielectric_idx, pid) }
void hit_emissive_push(uint pid)   { QUEUE_PUSH(Q_EMISSIVE,   hit_emissive_idx,   pid) }
void shadow_queue_push(uint pid)   { QUEUE_PUSH(Q_SHADOW,     shadow_queue_idx,   pid) }

#endif
