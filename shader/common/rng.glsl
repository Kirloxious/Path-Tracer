#ifndef RNG_GLSL
#define RNG_GLSL

const float MAT_LAMBERTIAN = 0.0;
const float MAT_METAL = 1.0;
const float MAT_DIELECTRIC = 2.0;
const float MAT_EMISSIVE = 3.0;

const float infinity = 1.0 / 0.0;
const float PI = 3.14159265358979323846;

// Seed an RNG state. Mixes pixel id and frame index so each pixel and frame
// gets an independent sequence. The | 1u guarantees a non-zero seed (xorshift
// stays at 0 if seeded with 0).
uvec4 init_rng(uint pid, int frame_index) {
    uint seed = (pid * 1973u + uint(frame_index) * 9277u + 1u) | 1u;
    return uvec4(seed, 0u, 0u, 0u);
}

uint xor_shift32(inout uint state) {
    uint x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

float random_unilateral(inout uint state) {
    return uintBitsToFloat((xor_shift32(state) >> 9) | 0x3F800000u) - 1.0;
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
