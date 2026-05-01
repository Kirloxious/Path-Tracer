#ifndef RESTIR_COMMON_GLSL
#define RESTIR_COMMON_GLSL

#include "rng.glsl"
#include "scene_buffers.glsl"

// Per-pixel reservoir for ReSTIR DI. Holds one resampled light sample plus
// the bookkeeping needed to combine reservoirs across frames (temporal) and
// neighbours (spatial). Size is 32 bytes; field order is fixed by std430.
//
// Conventions
//   light_tri_idx  index into TrianglesBuffer of the chosen emissive triangle.
//                  Sentinel UINT_MAX means "no valid sample" (e.g. pixel didn't
//                  hit anything, scene has no lights, or all candidates failed).
//   bary           barycentrics (u, v) on that triangle; the surface point is
//                  v0 + u*e1 + v*e2.
//   w_sum          running RIS weight sum across all candidates seen so far.
//   M              effective sample count. float so future temporal capping
//                  can fractionally clamp it.
//   W              unbiased RIS weight = w_sum / (M * target_pdf), or 0 when
//                  the chosen sample is occluded. Final shading evaluates
//                  contribution = f * Le * G * W (visibility folded into W=0).
//   target_pdf     p_hat at the chosen sample, *unshadowed* (luminance of
//                  f * Le * G). Kept post-finalize because spatial/temporal
//                  reuse re-evaluates p_hat at the receiving surface.
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

layout(std430, binding = 18) restrict buffer RestirReservoirsCurrent {
    Reservoir reservoirs[];
};

layout(std430, binding = 19) restrict readonly buffer RestirReservoirsPrev {
    Reservoir prev_reservoirs[];
};

// Spatial reuse input. The spatial shader reads from this and writes to
// `reservoirs[]` (binding 18). The two spatial passes ping-pong by swapping
// which underlying Buffer is bound to 18 vs 20 between dispatches.
layout(std430, binding = 20) restrict readonly buffer RestirSpatialInput {
    Reservoir spatial_input[];
};

float restir_luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// p_hat at receiver (P, N, albedo) for the light sample (tri_idx, bary).
// Returns 0 if light back-facing or receiver back-facing. Lambertian-only
// (matches v1 scope of restir_initial.comp).
float restir_target_pdf_lambertian(vec3 P, vec3 N, vec3 albedo, uint tri_idx, vec2 bary) {
    if (tri_idx == RESTIR_INVALID_TRI) return 0.0;

    Triangle tri = triangles[tri_idx];
    vec3 v0 = vertices[tri.indices.x].position;
    vec3 sp = v0 + bary.x * tri.e1 + bary.y * tri.e2;

    vec3  d         = sp - P;
    float dist2     = dot(d, d);
    float inv_dist  = inversesqrt(dist2);
    vec3  light_dir = d * inv_dist;

    float cross_dot = dot(cross(tri.e1, tri.e2), light_dir);
    if (cross_dot >= 0.0) return 0.0;

    float cos_theta = max(0.0, dot(N, light_dir));
    if (cos_theta <= 0.0) return 0.0;

    Material lmat = mats[tri.material_index];
    vec3  contrib_rgb = albedo * lmat.emission * cos_theta * (1.0 / PI);
    return restir_luminance(contrib_rgb);
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

// Streaming RIS update: present candidate (tri_idx, bary) with RIS weight w_i
// and target_pdf p_hat_i. Returns true if this candidate was selected.
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

// Biased Bitterli-style combine: present `other`'s chosen sample to `r`,
// weighted by other.W * other.M evaluated against the receiver's surface
// (via p_hat_at_self). Adds other.M to r.M unconditionally so the divisor
// in the final W reflects total sampling effort.
//
// Caller is responsible for re-evaluating p_hat_at_self at the same surface
// `r` represents; passing 0 lets the caller skip combination entirely (the
// candidate would never be selected anyway and 0×anything is fine here, but
// skipping early avoids polluting r.M with empty samples).
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

// After all candidates / combines are streamed, compute the unbiased weight W.
// Caller is responsible for separately zeroing W on visibility failure.
void reservoir_finalize(inout Reservoir r) {
    if (r.target_pdf > 0.0 && r.M > 0.0) {
        r.W = r.w_sum / (r.M * r.target_pdf);
    } else {
        r.W = 0.0;
    }
}

#endif
