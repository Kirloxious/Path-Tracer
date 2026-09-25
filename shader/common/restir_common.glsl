#ifndef RESTIR_COMMON_GLSL
#define RESTIR_COMMON_GLSL

#include "rng.glsl"
#include "scene_buffers.glsl"
#include "bsdf.glsl"
#include "lights.glsl"

// 32 bytes std430. W = w_sum / (Z * target_pdf), 0 when occluded; target_pdf is unshadowed.
struct Reservoir {
    uint  light_tri_idx;
    float M;
    vec2  bary;
    float w_sum;
    float W;
    float target_pdf;
    float _pad;
};

const uint RESTIR_INVALID_TRI = 0xFFFFFFFFu;

// pcg_seed() streams per kernel; spatial adds its pass index.
const uint RESTIR_RNG_INITIAL  = 0u;
const uint RESTIR_RNG_TEMPORAL = 1u;
const uint RESTIR_RNG_SPATIAL  = 2u;

// Any opaque non-mirror surface. restir_initial anchors on this and shade_surface consumes on it; two
// separate tests would let a reservoir describe a different vertex than the one shaded.
bool restir_can_anchor(Material m) {
    return (m.type == MAT_DIFFUSE || m.type == MAT_SPECULAR) && !bsdf_is_mirror(m);
}

// Luminance of f * Le * cos (full BRDF, so glossy receivers resample toward their highlight). G lives
// in the source pdf, and path throughput is constant per pixel so it drops out.
float restir_p_hat(vec3 N, vec3 V, uint matid, in Triangle light_tri, vec3 light_dir) {
    float cos_theta = dot(N, light_dir);
    if (!light_faces(light_tri, light_dir) || cos_theta <= 0.0) return 0.0;

    float ignored_pdf;
    vec3  f = bsdf_eval(mats[matid], N, V, light_dir, ignored_pdf);
    return luminance(f * material_emission(mats[light_tri.material_index]) * cos_theta);
}

float restir_target_pdf(vec3 P, vec3 N, vec3 V, uint matid, uint tri_idx, vec2 bary) {
    if (tri_idx == RESTIR_INVALID_TRI) return 0.0;
    Triangle tri = triangles[tri_idx];
    return restir_p_hat(N, V, matid, tri, normalize(light_point(tri, bary) - P));
}

void reservoir_clear(out Reservoir r) {
    r.light_tri_idx = RESTIR_INVALID_TRI;
    r.M             = 0.0;
    r.bary          = vec2(0.0);
    r.w_sum         = 0.0;
    r.W             = 0.0;
    r.target_pdf    = 0.0;
    r._pad          = 0.0;
}

// Streaming RIS update. Returns true if this candidate was selected.
bool reservoir_update(inout Reservoir r, uint tri_idx, vec2 bary, float p_hat_i, float w_i, inout uint rng) {
    r.M     += 1.0;
    r.w_sum += w_i;
    if (r.w_sum > 0.0 && random_unilateral(rng) < (w_i / r.w_sum)) {
        r.light_tri_idx = tri_idx;
        r.bary          = bary;
        r.target_pdf    = p_hat_i;
        return true;
    }
    return false;
}

// Weight is p_hat_at_self * other.W * other.M, p_hat re-evaluated at `r`'s surface. The caller owns Z.
bool reservoir_combine(inout Reservoir r, in Reservoir other, float p_hat_at_self, inout uint rng) {
    float w_i = p_hat_at_self * other.W * other.M;
    bool  selected = false;
    r.w_sum += w_i;
    if (r.w_sum > 0.0 && w_i > 0.0 && random_unilateral(rng) < (w_i / r.w_sum)) {
        r.light_tri_idx = other.light_tri_idx;
        r.bary          = other.bary;
        r.target_pdf    = p_hat_at_self;
        selected        = true;
    }
    r.M += other.M;
    return selected;
}

// Z sums M over only the reservoirs whose surface gives the winner non-zero p_hat (Bitterli 2020, Alg. 6);
// the full M would darken corners and silhouettes. Zeroing W on occlusion is the caller's job.
void reservoir_finalize(inout Reservoir r, float Z) {
    if (r.target_pdf > 0.0 && Z > 0.0) {
        r.W = r.w_sum / (Z * r.target_pdf);
    } else {
        r.W = 0.0;
    }
}

#endif
