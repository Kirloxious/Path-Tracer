#ifndef RESTIR_COMMON_GLSL
#define RESTIR_COMMON_GLSL

#include "rng.glsl"
#include "scene_buffers.glsl"
#include "bsdf.glsl"
#include "lights.glsl"

// Per-pixel ReSTIR DI reservoir, 32 bytes std430.
//   light_tri_idx  chosen emissive triangle, or RESTIR_INVALID_TRI
//   bary           its light_point() barycentrics
//   M              sample count behind the reservoir
//   W              contribution weight w_sum / (Z * target_pdf); 0 when the sample is occluded
//   target_pdf     the chosen sample's p_hat, unshadowed
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

// pcg_seed() streams for each kernel's reservoir acceptance tests; spatial adds its pass index.
const uint RESTIR_RNG_INITIAL  = 0u;
const uint RESTIR_RNG_TEMPORAL = 1u;
const uint RESTIR_RNG_SPATIAL  = 2u;

// Which surfaces can hold a reservoir: any opaque one whose BRDF an explicitly sampled light
// direction can reach, i.e. anything but a perfect mirror. restir_initial anchors on this and
// shade_surface consumes on it — two tests would let a reservoir silently describe a different
// vertex than the one being shaded.
bool restir_can_anchor(Material m) {
    return (m.type == MAT_DIFFUSE || m.type == MAT_SPECULAR) && !bsdf_is_mirror(m);
}

// Target pdf at receiver (N, V, matid) toward `light_dir` on `light_tri`: luminance of the
// integrand per unit solid angle, f * Le * cos. The geometry term lives in the source pdf the
// candidate was drawn with, and path throughput is constant per pixel so it drops out. 0 if
// the light is back-facing or the receiver faces away.
//
// The full BRDF rather than a Lambertian proxy, so a glossy receiver resamples toward the
// lights its highlight sees.
float restir_p_hat(vec3 N, vec3 V, uint matid, in Triangle light_tri, vec3 light_dir) {
    float cos_theta = dot(N, light_dir);
    if (!light_faces(light_tri, light_dir) || cos_theta <= 0.0) return 0.0;

    float ignored_pdf;
    vec3  f = bsdf_eval(mats[matid], N, V, light_dir, ignored_pdf);
    return luminance(f * material_emission(mats[light_tri.material_index]) * cos_theta);
}

// restir_p_hat for a stored sample (tri_idx, bary) seen from P.
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

// Streaming RIS update. @return true if this candidate was selected.
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

// Presents `other`'s sample to `r` with weight p_hat_at_self * other.W * other.M, where
// p_hat_at_self is its target pdf re-evaluated at the surface `r` describes. The caller owns Z;
// see reservoir_finalize().
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

// Computes W once every candidate and combine has been streamed.
//
// `Z` is the sum of M over the reservoirs whose own surface gives the winner a non-zero target
// pdf (Bitterli et al. 2020, Algorithm 6). The full M would also count partners that could never
// have produced it, and darken corners and silhouettes. Plain RIS at one surface passes r.M.
// Zeroing W on occlusion is the caller's job.
void reservoir_finalize(inout Reservoir r, float Z) {
    if (r.target_pdf > 0.0 && Z > 0.0) {
        r.W = r.w_sum / (Z * r.target_pdf);
    } else {
        r.W = 0.0;
    }
}

#endif
