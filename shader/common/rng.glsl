#ifndef RNG_GLSL
#define RNG_GLSL

//=============================================================================
// Hash / white-noise stream
//=============================================================================

// PCG, RXS-M-XS output (O'Neill, "PCG: A Family of Simple Fast Space-Efficient Statistically
// Good Algorithms for Random Number Generation"). Also the hash under sampler_mix().
uint pcg_advance(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_unilateral(inout uint state) {
    return uintBitsToFloat((pcg_advance(state) >> 9) | 0x3F800000u) - 1.0;
}

//=============================================================================
// Low-discrepancy sampler
//=============================================================================

// Owen-scrambled Sobol' (Burley 2020, "Practical Hash-Based Owen Scrambling"): stratified
// across samples, randomized per pixel, still unbiased.
//
// The sample index is the accumulation frame, so the stratified set is one pixel's samples
// *across frames* and `seed` must not vary with the frame — swapping sampler_seed() for
// pcg_seed() silently reduces this to white noise.
//
// Dimensions are padded: each 2D group gets its own shuffle and scramble of the same sequence,
// which keeps every group a proper stratified set where a high-dimensional Sobol' would degrade.
struct Sampler {
    uint seed;   // per-pixel, fixed for the run
    uint index;  // sample index — the accumulation frame
    uint dim;    // dimension group, bumped by each draw
};

uint sampler_mix(uint a, uint b) {
    uint h = a ^ (b * 0x9e3779b9u);
    return pcg_advance(h);
}

// Seed for a white-noise stream, fresh every frame. `stream` keeps two kernels drawing in one
// frame from sharing a sequence.
uint pcg_seed(uint pid, uint time_seed, uint stream) {
    return sampler_mix(sampler_mix(pid, time_seed), stream);
}

// Nested uniform scramble: a few ALU ops standing in for a full Owen scramble, which would
// otherwise need the sample set materialized. Constants from Vegdahl's improved
// Laine-Karras hash ("Building a Better LK Hash", 2021).
uint sampler_owen(uint x, uint seed) {
    x = bitfieldReverse(x);
    x ^= x * 0x3d20adeau;
    x += seed;
    x *= (seed >> 16) | 1u;
    x ^= x * 0x05526c56u;
    x ^= x * 0x53a22864u;
    return bitfieldReverse(x);
}

// First two Sobol' dimensions. The first is the van der Corput sequence, which is just a bit
// reversal; the second's direction numbers are what the v ^= v >> 1 ladder generates.
uvec2 sobol_2d(uint index) {
    uint y = 0u;
    uint v = 0x80000000u;
    for (uint i = index; i != 0u; i >>= 1u, v ^= v >> 1u) {
        if ((i & 1u) != 0u) {
            y ^= v;
        }
    }
    return uvec2(bitfieldReverse(index), y);
}

// [0, 1) with 24 bits. Scaling all 32 bits rounds to exactly 1.0 near the top, which every
// caller indexing an array by `u * count` relies on never happening.
float sampler_unilateral(uint x) {
    return float(x >> 8) * (1.0 / 16777216.0);
}

vec2 sampler_2d(inout Sampler s) {
    uint  g = s.dim++;
    uvec2 v = sobol_2d(sampler_owen(s.index, sampler_mix(s.seed, g * 3u)));
    return vec2(sampler_unilateral(sampler_owen(v.x, sampler_mix(s.seed, g * 3u + 1u))),
                sampler_unilateral(sampler_owen(v.y, sampler_mix(s.seed, g * 3u + 2u))));
}

float sampler_1d(inout Sampler s) {
    uint g        = s.dim++;
    uint shuffled = sampler_owen(s.index, sampler_mix(s.seed, g * 3u));
    return sampler_unilateral(sampler_owen(bitfieldReverse(shuffled), sampler_mix(s.seed, g * 3u + 1u)));
}

// Dimension groups reserved per path vertex; see shade_surface for the layout within one.
const uint SAMPLER_DIMS_PER_BOUNCE = 12u;

// `frame` is frame_index, which counts from 1. The sample index must count from 0: a
// shuffled Sobol' prefix is a complete stratified net only over [0, 2^k).
Sampler sampler_init(uint seed, int frame, uint dim_base) {
    Sampler s;
    s.seed  = seed;
    s.index = uint(max(frame - 1, 0));
    s.dim   = dim_base;
    return s;
}

// Pins the next draw to a known group, so a draw after a branch lands on the same dimension
// every frame whichever way the branch went.
void sampler_set_dim(inout Sampler s, uint dim) {
    s.dim = dim;
}

// Per-pixel seed. Varies per run so frame 1 differs between launches, but *not* per frame.
uint sampler_seed(uint pid, uint time_seed) {
    return sampler_mix(pid, time_seed + 0x736ee2b1u);
}

#endif
