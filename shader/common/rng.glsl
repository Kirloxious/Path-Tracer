#ifndef RNG_GLSL
#define RNG_GLSL

// ---- Hash / white-noise stream ----

// PCG RXS-M-XS (O'Neill). Also the hash under sampler_mix().
uint pcg_advance(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_unilateral(inout uint state) {
    return uintBitsToFloat((pcg_advance(state) >> 9) | 0x3F800000u) - 1.0;
}

// ---- Low-discrepancy sampler ----

// Owen-scrambled Sobol' (Burley 2020), padded per 2D group. The sample index is the accumulation frame,
// so `seed` must not vary per frame: swapping in pcg_seed() silently degrades this to white noise.
struct Sampler {
    uint seed; // fixed for the accumulation
    uint index; // the accumulation frame
    uint dim;
};

uint sampler_mix(uint a, uint b) {
    uint h = a ^ (b * 0x9e3779b9u);
    return pcg_advance(h);
}

// `stream` keeps two kernels drawing in one frame from sharing a sequence.
uint pcg_seed(uint pid, uint time_seed, uint stream) {
    return sampler_mix(sampler_mix(pid, time_seed), stream);
}

// Nested uniform scramble standing in for a full Owen scramble (Vegdahl's improved LK hash, 2021).
uint sampler_owen(uint x, uint seed) {
    x = bitfieldReverse(x);
    x ^= x * 0x3d20adeau;
    x += seed;
    x *= (seed >> 16) | 1u;
    x ^= x * 0x05526c56u;
    x ^= x * 0x53a22864u;
    return bitfieldReverse(x);
}

// Dimension 0 is van der Corput (a bit reversal); the v ^= v >> 1 ladder generates dimension 1.
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

// 24 bits: scaling all 32 can round to exactly 1.0, which `u * count` indexing relies on never seeing.
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

const uint SAMPLER_DIMS_PER_BOUNCE = 12u;

// frame_index counts from 1, but a shuffled Sobol' prefix is only a complete net over [0, 2^k).
Sampler sampler_init(uint seed, int frame, uint dim_base) {
    Sampler s;
    s.seed  = seed;
    s.index = uint(max(frame - 1, 0));
    s.dim   = dim_base;
    return s;
}

// Pins the next draw to a known group so it lands on the same dimension whichever way a branch went.
void sampler_set_dim(inout Sampler s, uint dim) {
    s.dim = dim;
}

// Varies per run so frame 1 differs between launches, but *not* per frame.
uint sampler_seed(uint pid, uint time_seed) {
    return sampler_mix(pid, time_seed + 0x736ee2b1u);
}

#endif
