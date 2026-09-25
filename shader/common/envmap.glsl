#ifndef ENVMAP_GLSL
#define ENVMAP_GLSL

#include "math.glsl"
#include "constants.glsl"

layout(binding = TEX_ENV_MAP) uniform sampler2D env_map_tex;

// Alias table over a downsample weighted by radiance x equirect Jacobian. envmap_sample() and
// envmap_pdf() must agree exactly, since MIS weights by their ratio.
struct EnvSampleCell {
    float accept;
    uint  alias;
    float pdf_num;  // solid-angle pdf with sin(theta) factored out
    float _pad;
};

layout(std430, binding = BIND_ENV_SAMPLES) readonly buffer EnvSampleBuffer { EnvSampleCell env_cells[]; };

// sin(theta) floor for the poles, where the pdf's 1/sin would otherwise diverge.
const float ENV_MIN_SIN_THETA = 1e-4;

// Equirect, Y up, phi = 0 at +X. These two must stay exact inverses: the pdf is looked up by converting
// the sampled direction back.
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

float envmap_pdf(vec3 dir) {
    if (env_map_valid == 0) return 0.0;
    vec2  uv = envmap_dir_to_uv(dir);
    ivec2 c  = clamp(ivec2(uv * vec2(env_sample_size)), ivec2(0), env_sample_size - 1);
    return env_cells[c.y * env_sample_size.x + c.x].pdf_num / envmap_sin_theta(dir);
}

/// `u_cell`.x picks the cell, .y runs the alias test; `u_jitter` places the sample inside it.
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
