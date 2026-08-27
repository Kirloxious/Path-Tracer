#ifndef ENVMAP_GLSL
#define ENVMAP_GLSL

#include "rng.glsl"  // for PI

// Sampled by generate.comp (primary sky) and trace.comp (continuation miss).
// v1 is miss-sampling only — no NEE toward the env, no importance-sampled CDF.
// This means an overcast HDR converges in seconds; a sunny HDR fireflies until
// env-IS lands (deferred to a future pass).
layout(binding = 9) uniform sampler2D env_map_tex;
layout(location = 20) uniform int   env_map_valid;      // 0 = no envmap bound → return black
layout(location = 21) uniform float env_map_intensity;  // per-scene scale

// Equirectangular (lat-long) lookup. Input direction must be unit length.
// Y-up convention matches the rest of the tracer (glm::vec3(0, 1, 0) = vup).
vec3 sample_envmap(vec3 dir) {
    if (env_map_valid == 0) return vec3(0.0);
    // phi ∈ [-π, π] maps to u ∈ [0, 1]; theta ∈ [0, π] maps to v ∈ [0, 1].
    // atan(dir.z, dir.x) puts phi=0 at +X, +π/2 at +Z — an arbitrary but consistent yaw.
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    vec2  uv    = vec2(phi / (2.0 * PI) + 0.5, theta / PI);
    return textureLod(env_map_tex, uv, 0.0).rgb * env_map_intensity;
}

#endif
