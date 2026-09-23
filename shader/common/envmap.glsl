#ifndef ENVMAP_GLSL
#define ENVMAP_GLSL

#include "rng.glsl"  // for PI
#include "uniform_locations.glsl"

// The environment as a distant area light: read on a miss (generate.comp for the primary sky,
// trace.comp for a continuation) and sampled explicitly by shade_surface's NEE.
layout(binding = 9) uniform sampler2D env_map_tex;
layout(location = LOC_ENV_MAP_VALID) uniform int   env_map_valid;      // 0 = no envmap bound → return black
layout(location = LOC_ENV_MAP_INTENSITY) uniform float env_map_intensity;  // per-scene scale

// Importance-sampling grid: a Vose alias table over a downsampled copy of the map, weighted by
// radiance times the equirect Jacobian. Built in EnvMap::buildSamplingTable.
//
// Without it the environment is reachable only by rays that happen to escape, so a map whose
// energy is concentrated in a small bright sun fireflies indefinitely — the estimator has to
// find a few hundred pixels of sky by chance. The tradeoff is that the sampled density must
// agree exactly with envmap_pdf() below, since MIS weights every contribution by their ratio.
struct EnvSampleCell {
    float accept;
    uint  alias;
    float pdf_num;  // solid-angle pdf with sin(theta) factored out
    float _pad;
};

layout(std430, binding = 27) readonly buffer EnvSampleBuffer { EnvSampleCell env_cells[]; };
layout(location = LOC_ENV_SAMPLE_SIZE) uniform ivec2 env_sample_size;

// sin(theta) floor for the poles, where the pdf's 1/sin would otherwise diverge.
const float ENV_MIN_SIN_THETA = 1e-4;

// Equirectangular (lat-long) lookup. Input direction must be unit length.
// Y-up convention matches the rest of the tracer (glm::vec3(0, 1, 0) = vup).
// phi ∈ [-π, π] maps to u ∈ [0, 1]; theta ∈ [0, π] maps to v ∈ [0, 1].
// atan(dir.z, dir.x) puts phi=0 at +X, +π/2 at +Z — an arbitrary but consistent yaw.
//
// These two are exact inverses, and the sampler depends on that: it draws a uv, converts to a
// direction, and its pdf is looked up by converting back. Any drift between them mis-weights
// every environment sample in the image.
vec2 envmap_dir_to_uv(vec3 dir) {
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI);
}

vec3 envmap_uv_to_dir(vec2 uv) {
    float phi   = (uv.x - 0.5) * 2.0 * PI;
    float theta = uv.y * PI;
    float st    = sin(theta);
    return vec3(st * cos(phi), cos(theta), st * sin(phi));
}

vec3 sample_envmap(vec3 dir) {
    if (env_map_valid == 0) return vec3(0.0);
    return textureLod(env_map_tex, envmap_dir_to_uv(dir), 0.0).rgb * env_map_intensity;
}

float envmap_sin_theta(vec3 dir) {
    return max(sqrt(max(0.0, 1.0 - dir.y * dir.y)), ENV_MIN_SIN_THETA);
}

/// Solid-angle density this sampler would have used for `dir`. The BSDF side of MIS needs it
/// to weight a ray that escaped to the sky against the NEE sample that could have found it.
float envmap_pdf(vec3 dir) {
    if (env_map_valid == 0) return 0.0;
    vec2  uv = envmap_dir_to_uv(dir);
    ivec2 c  = clamp(ivec2(uv * vec2(env_sample_size)), ivec2(0), env_sample_size - 1);
    return env_cells[c.y * env_sample_size.x + c.x].pdf_num / envmap_sin_theta(dir);
}

/// Draws a direction proportional to radiance times the Jacobian. `u_cell` picks the grid cell
/// (x selects, y runs the alias test); `u_jitter` places the sample inside it.
/// @return the direction; `out_pdf` is 0 when there is nothing to sample.
vec3 envmap_sample(vec2 u_cell, vec2 u_jitter, out vec3 out_radiance, out float out_pdf) {
    out_radiance = vec3(0.0);
    out_pdf      = 0.0;
    if (env_map_valid == 0) return vec3(0.0, 1.0, 0.0);

    int           n    = env_sample_size.x * env_sample_size.y;
    int           idx  = min(int(u_cell.x * float(n)), n - 1);
    EnvSampleCell cell = env_cells[idx];
    if (u_cell.y >= cell.accept) {
        idx  = int(cell.alias);
        cell = env_cells[idx];
    }

    vec2 uv  = (vec2(idx % env_sample_size.x, idx / env_sample_size.x) + u_jitter) / vec2(env_sample_size);
    vec3 dir = envmap_uv_to_dir(uv);

    out_pdf      = cell.pdf_num / envmap_sin_theta(dir);
    out_radiance = textureLod(env_map_tex, uv, 0.0).rgb * env_map_intensity;
    return dir;
}

#endif
