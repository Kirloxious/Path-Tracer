#ifndef BSDF_GLSL
#define BSDF_GLSL

#include "primitives.glsl"
#include "rng.glsl"

// Metallic-roughness BSDF: Lambert diffuse + GGX/Trowbridge-Reitz specular, mirroring
// src/scene/material.h. Shared by every kernel that shades a surface and by ReSTIR's target
// pdf, so there is exactly one definition of "what does this material do with light".
//
// Conventions throughout: N is the shading normal, V points from the surface toward the
// viewer, L toward the light. All are unit length and in world space. `roughness` is
// perceptual — alpha = roughness^2.

// Below this alpha the specular lobe is treated as a Dirac delta rather than a very sharp
// GGX. Two reasons, both load-bearing:
//   - D_GGX diverges as alpha -> 0 (D ~ 1/alpha^2), so a near-zero roughness produces
//     enormous D and pdf values that blow up the estimator.
//   - restir_initial's mirror walk follows plain reflect(). It can only stay in lockstep
//     with the shading kernels if they agree, exactly, on which surfaces are mirrors.
// alpha < 1e-3 is roughness < ~0.032; Material::Metal(c, 0) lands at exactly 0.
const float BSDF_DELTA_ALPHA = 1e-3;

float bsdf_luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float bsdf_alpha(Material m) {
    return m.roughness * m.roughness;
}

/// @return true when this material's specular lobe is a perfect mirror (delta).
bool bsdf_is_delta(Material m) {
    return bsdf_alpha(m) < BSDF_DELTA_ALPHA;
}

/// Normal-incidence Fresnel. Dielectrics derive it from ior; conductors tint it with base_color.
vec3 bsdf_f0(Material m) {
    float f0_dielectric = (m.ior - 1.0) / (m.ior + 1.0);
    f0_dielectric *= f0_dielectric;
    return mix(vec3(f0_dielectric), m.base_color, m.metallic);
}

/// Conductors have no diffuse lobe; the transmissive fraction does not scatter diffusely either.
vec3 bsdf_diffuse_albedo(Material m) {
    return m.base_color * (1.0 - m.metallic) * (1.0 - m.transmission);
}

/// @return true for a perfect mirror: a delta specular lobe and *no* diffuse lobe beside it.
///
/// Distinct from bsdf_is_delta. A smooth dielectric (metallic 0, roughness 0) has a delta coat
/// over a diffuse base; only a pure mirror is invisible to NEE, and only a pure mirror is a
/// deterministic scatter that restir_initial's reflect() walk can follow.
bool bsdf_is_mirror(Material m) {
    vec3 d = bsdf_diffuse_albedo(m);
    return bsdf_is_delta(m) && max(d.r, max(d.g, d.b)) <= 0.0;
}

float bsdf_D_ggx(float NoH, float alpha) {
    float a2 = alpha * alpha;
    float d  = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-9);
}

/// Height-correlated Smith visibility, already folded with the 1 / (4 NoL NoV) denominator.
float bsdf_V_smith(float NoV, float NoL, float alpha) {
    float a2 = alpha * alpha;
    float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1e-7);
}

/// Smith masking for a single direction, used by the VNDF pdf.
float bsdf_G1_smith(float NoV, float alpha) {
    float a2 = alpha * alpha;
    return 2.0 * NoV / max(NoV + sqrt(NoV * NoV * (1.0 - a2) + a2), 1e-7);
}

/// Schlick Fresnel for a dielectric interface, parameterized by the relative IOR the ray is
/// crossing into (eta = ior_from / ior_to). Lives here rather than in the transmissive kernel
/// so the reflection and refraction paths cannot drift apart. Returns 1 under total internal
/// reflection.
float bsdf_fresnel_dielectric(float cosine, float eta) {
    cosine = clamp(cosine, 0.0, 1.0);
    // Leaving the denser medium, Schlick must take the transmitted cosine: on the incident one
    // F sits near r0 right up to the critical angle and then jumps to 1.
    if (eta > 1.0) {
        float sin2_t = eta * eta * (1.0 - cosine * cosine);
        if (sin2_t >= 1.0) {
            return 1.0;
        }
        cosine = sqrt(1.0 - sin2_t);
    }
    float r0 = (1.0 - eta) / (1.0 + eta);
    r0 = r0 * r0;
    float x  = 1.0 - cosine;
    float x2 = x * x;
    return r0 + (1.0 - r0) * x2 * x2 * x;
}

vec3 bsdf_F_schlick(vec3 f0, float u) {
    return f0 + (1.0 - f0) * pow(1.0 - u, 5.0);
}

/// Branchless orthonormal basis (Duff et al., "Building an Orthonormal Basis, Revisited").
void bsdf_onb(vec3 n, out vec3 t, out vec3 b) {
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float c = n.x * n.y * a;
    t = vec3(1.0 + s * n.x * n.x * a, s * c, -s * n.x);
    b = vec3(c, s + n.y * n.y * a, -n.y);
}

/// Probability of choosing the specular lobe, weighting each lobe by the reflectance it
/// actually has at this view angle.
///
/// The Fresnel term is what makes that view-dependent, and it matters at grazing incidence:
/// there a dielectric reflects nearly everything and the diffuse lobe carries the matching
/// (1 - F), so a view-independent split keeps spending half its samples on a lobe already
/// scaled to nothing. F >= f0 >= 0.04 for a dielectric, which subsumes the explicit floor
/// this used to need. A conductor has no diffuse albedo, so it still returns exactly 1.
///
/// bsdf_eval and bsdf_sample must pass the same NoV, or the density that is evaluated stops
/// describing the procedure that sampled it.
float bsdf_spec_prob(Material m, float NoV) {
    float ws = bsdf_luminance(bsdf_F_schlick(bsdf_f0(m), NoV));
    float wd = bsdf_luminance(bsdf_diffuse_albedo(m)) * (1.0 - ws);
    return ws / max(wd + ws, 1e-6);
}

/// Karis's analytic fit to the split-sum DFG integral (Physically Based Shading in Mobile,
/// SIGGRAPH 2014). Used only for the energy-compensation term below, which is why an
/// approximation is fine — it saves a precomputed LUT and the texture binding it would cost.
vec2 bsdf_env_brdf_approx(float NoV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4  r    = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

/// Kulla-Conty multiple-scattering compensation.
///
/// A single-scatter GGX lobe only accounts for light that hits one microfacet and leaves.
/// Light that bounces between microfacets before escaping is dropped, so the surface loses
/// energy — negligible when smooth, but a rough conductor can shed well over a third of its
/// reflectance and reads as implausibly dark. This scales the lobe by the reciprocal of its
/// directional albedo to put that energy back.
vec3 bsdf_energy_compensation(vec3 f0, float NoV, float roughness) {
    vec2  ab  = bsdf_env_brdf_approx(NoV, roughness);
    float Ess = ab.x + ab.y;
    return vec3(1.0) + f0 * (1.0 / max(Ess, 1e-3) - 1.0);
}

/// Evaluates f_r(V, L) and the combined sampling pdf for an explicitly chosen direction.
/// A delta specular lobe cannot be hit that way, so it contributes nothing here: a pure
/// mirror returns 0, and a smooth dielectric returns only its diffuse base.
vec3 bsdf_eval(Material m, vec3 N, vec3 V, vec3 L, out float pdf) {
    pdf = 0.0;
    if (bsdf_is_mirror(m)) {
        return vec3(0.0);
    }

    float NoL = dot(N, L);
    float NoV = dot(N, V);
    if (NoL <= 0.0 || NoV <= 0.0) {
        return vec3(0.0);
    }

    if (bsdf_is_delta(m)) {
        // The coat reflects F(NoV) along the mirror direction, so the base receives the rest.
        float ps = bsdf_spec_prob(m, NoV);
        pdf      = (1.0 - ps) * (NoL / PI);
        return (vec3(1.0) - bsdf_F_schlick(bsdf_f0(m), NoV)) * bsdf_diffuse_albedo(m) * (1.0 / PI);
    }

    vec3  H   = normalize(V + L);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    float alpha = bsdf_alpha(m);
    vec3  F     = bsdf_F_schlick(bsdf_f0(m), VoH);
    float D     = bsdf_D_ggx(NoH, alpha);

    vec3 specular = F * D * bsdf_V_smith(NoV, NoL, alpha) * bsdf_energy_compensation(bsdf_f0(m), NoV, m.roughness);
    // (1 - F) keeps the pair energy-conserving: what the specular lobe reflects cannot also
    // enter the diffuse one.
    vec3 diffuse = (vec3(1.0) - F) * bsdf_diffuse_albedo(m) * (1.0 / PI);

    float ps = bsdf_spec_prob(m, NoV);
    pdf = ps * (D * bsdf_G1_smith(NoV, alpha) / (4.0 * NoV)) + (1.0 - ps) * (NoL / PI);

    return diffuse + specular;
}

/// Reflection lobe of a rough dielectric, evaluated for an explicitly chosen direction.
///
/// Transmission is deliberately not covered: connecting a light to the viewer *through* a
/// refracting interface is a constrained-path problem, so a refracted direction correctly gets
/// zero density here and stays the business of BSDF sampling alone.
///
/// The reflect/refract split is a Fresnel-weighted coin flip, so F is both this lobe's weight
/// and the probability the sampler would have branched into it — which is why it appears in
/// the pdf as well as the value.
///
/// @param eta Relative IOR being crossed: 1/ior entering, ior leaving.
vec3 bsdf_eval_dielectric_reflection(Material m, vec3 N, vec3 V, vec3 L, float eta, out float pdf) {
    pdf = 0.0;
    if (bsdf_is_delta(m)) {
        return vec3(0.0);
    }

    float NoL = dot(N, L);
    float NoV = dot(N, V);
    if (NoL <= 0.0 || NoV <= 0.0) {
        return vec3(0.0);
    }

    vec3  H   = normalize(V + L);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    float alpha = bsdf_alpha(m);
    float F     = bsdf_fresnel_dielectric(VoH, eta);
    float D     = bsdf_D_ggx(NoH, alpha);

    pdf = F * D * bsdf_G1_smith(NoV, alpha) / (4.0 * NoV);
    return vec3(F * D * bsdf_V_smith(NoV, NoL, alpha));
}

/// Cosine-weighted direction about N, by Malley's method: a concentric point on the disc
/// lifted to the hemisphere. The pdf is unchanged at NoL / PI, but unlike `N + a random unit
/// vector` — also cosine-distributed — this is a smooth warp of the unit square, so a
/// stratified input still covers the hemisphere evenly once warped.
vec3 bsdf_sample_cosine(vec3 N, vec2 u) {
    float r   = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    vec3  T, B;
    bsdf_onb(N, T, B);
    return normalize(r * cos(phi) * T + r * sin(phi) * B + sqrt(max(0.0, 1.0 - u.x)) * N);
}

/// Sampled-visible-normal distribution sampling (Heitz 2018). `Ve` is the view direction in
/// the local frame where N = +z; returns a half vector in that same frame.
vec3 bsdf_sample_vndf(vec3 Ve, float alpha, vec2 u) {
    vec3  Vh    = normalize(vec3(alpha * Ve.x, alpha * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3  T1    = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3  T2    = cross(Vh, T1);

    float r   = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float t1  = r * cos(phi);
    float t2  = r * sin(phi);
    float s   = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(max(0.0, 1.0 - t1 * t1)) + s * t2;

    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    return normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
}

/// One BSDF sample.
///   out_L      sampled direction
///   out_weight f * cos / pdf — multiply straight into throughput
///   out_pdf    solid-angle pdf, or 0 for a delta lobe (nothing to MIS against)
///   out_delta  true when the sample came from a delta lobe
/// @return false when the sample is unusable and the path should die.
bool bsdf_sample(Material m, vec3 N, vec3 V, inout Sampler smp, out vec3 out_L, out vec3 out_weight, out float out_pdf, out bool out_delta) {
    out_L      = vec3(0.0);
    out_weight = vec3(0.0);
    out_pdf    = 0.0;
    out_delta  = false;

    float NoV = dot(N, V);
    if (NoV <= 0.0) {
        return false;
    }

    if (bsdf_is_delta(m)) {
        // A pure mirror skips the coin flip, keeping its scatter exactly the reflect() that
        // restir_initial's walk reproduces.
        float ps = bsdf_is_mirror(m) ? 1.0 : bsdf_spec_prob(m, NoV);
        if (ps >= 1.0 || sampler_1d(smp) < ps) {
            out_L = reflect(-V, N);
            if (dot(out_L, N) <= 0.0) {
                return false;
            }
            // Delta lobe: the D and G terms cancel against the pdf, leaving only Fresnel.
            out_weight = bsdf_F_schlick(bsdf_f0(m), NoV) / ps;
            out_pdf    = 0.0;
            out_delta  = true;
            return true;
        }
        out_L     = bsdf_sample_cosine(N, sampler_2d(smp));
        float NoL = dot(N, out_L);
        if (NoL <= 0.0) {
            return false;
        }
        vec3 f = bsdf_eval(m, N, V, out_L, out_pdf);
        if (out_pdf <= 0.0) {
            return false;
        }
        out_weight = f * NoL / out_pdf;
        return true;
    }

    float alpha = bsdf_alpha(m);
    if (sampler_1d(smp) < bsdf_spec_prob(m, NoV)) {
        vec3 T, B;
        bsdf_onb(N, T, B);
        vec3 Vl = vec3(dot(V, T), dot(V, B), NoV);
        vec3 Hl = bsdf_sample_vndf(Vl, alpha, sampler_2d(smp));
        vec3 H  = normalize(Hl.x * T + Hl.y * B + Hl.z * N);
        out_L   = reflect(-V, H);
    } else {
        out_L = bsdf_sample_cosine(N, sampler_2d(smp));
    }

    float NoL = dot(N, out_L);
    if (NoL <= 0.0) {
        return false;
    }

    // Evaluate against the *combined* pdf rather than the lobe we happened to draw from —
    // one-sample MIS across the two lobes, which stays unbiased and kills the fireflies a
    // per-lobe pdf produces where the two overlap.
    vec3 f = bsdf_eval(m, N, V, out_L, out_pdf);
    if (out_pdf <= 0.0) {
        return false;
    }
    out_weight = f * NoL / out_pdf;
    return true;
}

/// Samples a (possibly rough) dielectric interface — reflection or refraction.
///
/// Microfacet transmission after Walter et al. 2007, "Microfacet Models for Refraction through
/// Rough Surfaces". The half vector is drawn from the visible-normal distribution and the
/// reflect/refract branch is a Fresnel-weighted coin flip, so D, F and the refraction Jacobian
/// all cancel out of the estimator and the weight collapses to the Smith masking-shadowing
/// ratio G2/G1.
///
/// A perfectly smooth interface (`bsdf_is_delta`) takes H = N, which reduces this to exactly
/// the reflect/refract coin flip the kernel used before roughness meant anything here.
///
/// @param eta             Relative IOR being crossed into: 1/ior entering, ior leaving.
/// @param out_transmitted true when the ray passed through rather than bouncing off, which the
///                        caller needs in order to offset the new origin to the far side.
/// @param out_pdf         solid-angle density of a *reflected* sample, for MIS against NEE on
///                        the same lobe. Zero when the ray refracted or the interface is
///                        smooth: no explicit direction sample competes for either, so an
///                        emissive hit down those branches takes full weight.
/// @return false when the sample is unusable and the path should die.
bool bsdf_sample_transmissive(Material m, vec3 N, vec3 V, float eta, inout Sampler smp, out vec3 out_L, out vec3 out_weight,
                              out bool out_transmitted, out float out_pdf) {
    out_L           = vec3(0.0);
    out_weight      = vec3(1.0);
    out_transmitted = false;
    out_pdf         = 0.0;

    float NoV = dot(N, V);
    if (NoV <= 0.0) {
        return false;
    }

    float alpha = bsdf_alpha(m);
    bool  delta = bsdf_is_delta(m);

    vec3 H = N;
    if (!delta) {
        vec3 T, B;
        bsdf_onb(N, T, B);
        vec3 Vl = vec3(dot(V, T), dot(V, B), NoV);
        vec3 Hl = bsdf_sample_vndf(Vl, alpha, sampler_2d(smp));
        H       = normalize(Hl.x * T + Hl.y * B + Hl.z * N);
    }

    float VoH = dot(V, H);
    if (VoH <= 0.0) {
        return false;
    }

    float F = bsdf_fresnel_dielectric(VoH, eta);

    if (sampler_1d(smp) < F) {
        out_L = reflect(-V, H);
        if (!delta) {
            // Must match bsdf_eval_dielectric_reflection's pdf exactly — one is the density
            // NEE weights against, the other the density that produced this sample.
            out_pdf = F * bsdf_D_ggx(max(dot(N, H), 0.0), alpha) * bsdf_G1_smith(NoV, alpha) / (4.0 * NoV);
        }
    } else {
        out_L = refract(-V, H, eta);
        // Floating-point edge case: the Fresnel's TIR test can disagree with refract()'s
        // internal test near grazing angles, and refract() then returns vec3(0). A zero
        // direction in the ray queue makes inv_dir NaN in BVH traversal, so fall back to
        // reflection — the interface is effectively in TIR by either test.
        if (dot(out_L, out_L) < 1e-12) {
            out_L = reflect(-V, H);
        } else {
            out_transmitted = true;
            // Absorption is not applied here. It depends on how far the ray travels inside the
            // medium, which this function cannot know; shade_transmissive applies Beer-Lambert
            // over that segment when the ray reaches the far side.
        }
    }

    // A sampled microfacet can produce a direction on the wrong side of the macro surface.
    float NoL = dot(N, out_L);
    if (out_transmitted ? (NoL >= 0.0) : (NoL <= 0.0)) {
        return false;
    }

    if (!delta) {
        float absNoL = abs(NoL);
        float G2     = bsdf_V_smith(NoV, absNoL, alpha) * 4.0 * NoV * absNoL;
        float G1     = bsdf_G1_smith(NoV, alpha);
        out_weight *= G2 / max(G1, 1e-7);
    }
    return true;
}

#endif
