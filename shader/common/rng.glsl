#ifndef RNG_GLSL
#define RNG_GLSL

const float MAT_LAMBERTIAN = 0.0;
const float MAT_METAL = 1.0;
const float MAT_DIELECTRIC = 2.0;
const float MAT_EMISSIVE = 3.0;

const float infinity = 1.0 / 0.0;
const float PI = 3.14159265358979323846;

// Seed an RNG state. Mixes pixel id, frame index, and a per-run `time` seed so
// (a) adjacent pixels get independent sequences, (b) successive frames get
// fresh sequences, (c) different program runs produce different patterns —
// without (c), frame 1 looks identical every launch.
//
// PCG handles a zero state fine (it advances multiplicatively), so we drop the
// | 1u that xorshift required.
uvec4 init_rng(uint pid, int frame_index, uint time_seed) {
    uint seed = pid * 1973u + uint(frame_index) * 9277u + time_seed * 26699u + 1u;
    return uvec4(seed, 0u, 0u, 0u);
}

// PCG output XSH-RR variant (Melissa O'Neill, "PCG: A Family of Simple Fast
// Space-Efficient Statistically Good Algorithms for Random Number Generation").
// Replaces xorshift32 — xorshift produced visible structured noise when
// neighbouring kernels seeded their state with small additive offsets, since
// xorshift takes many iterations to fully decorrelate two close starting states.
uint pcg_advance(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_unilateral(inout uint state) {
    return uintBitsToFloat((pcg_advance(state) >> 9) | 0x3F800000u) - 1.0;
}

float random_bilateral(inout uint state) {
    return 2.0 * random_unilateral(state) - 1.0;
}

vec3 random_vec3(inout uint state) {
    return vec3(random_bilateral(state), random_bilateral(state), random_bilateral(state));
}

vec3 random_unit_vector(inout uint state) {
    float z = random_bilateral(state);
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * PI * random_unilateral(state);
    return vec3(r * cos(phi), r * sin(phi), z);
}

vec3 random_on_hemisphere(inout uint state, vec3 normal) {
    vec3 direction = random_unit_vector(state);
    return (dot(direction, normal) > 0.0) ? direction : -direction;
}

#endif
