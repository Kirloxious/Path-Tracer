#ifndef BSDF_GLSL
#define BSDF_GLSL

#include "primitives.glsl"
#include "math.glsl"
#include "rng.glsl"

// Metallic-roughness BSDF: Lambert diffuse + GGX specular, mirroring src/scene/material.h.
//
// N is the shading normal, V points toward the viewer, L toward the light; all unit length,
// world space. `roughness` is perceptual — alpha = roughness^2.

// Below this alpha the specular lobe is a Dirac delta: D_GGX diverges as alpha -> 0, and
// restir_initial's reflect() walk stays in lockstep with the shading kernels only if they
// agree exactly on which surfaces are mirrors. alpha < 1e-3 is roughness < ~0.032.
const float BSDF_DELTA_ALPHA = 1e-3;

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

/// Schlick Fresnel for a dielectric interface, eta = ior_from / ior_to. Returns 1 under total
/// internal reflection.
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

/// Probability of choosing the specular lobe, weighting each lobe by its reflectance at this view
/// angle: at grazing incidence a dielectric reflects nearly everything, and a fixed split would
/// spend half its samples on a diffuse lobe scaled to nothing. A conductor returns exactly 1.
///
/// bsdf_eval and bsdf_sample must pass the same NoV, or the evaluated density stops describing
/// the sampler.
float bsdf_spec_prob(Material m, float NoV) {
    float ws = luminance(bsdf_F_schlick(bsdf_f0(m), NoV));
    float wd = luminance(bsdf_diffuse_albedo(m)) * (1.0 - ws);
    return ws / max(wd + ws, 1e-6);
}

/// Karis's analytic fit to the split-sum DFG integral (Physically Based Shading in Mobile,
/// SIGGRAPH 2014) — accurate enough for the energy compensation below, and saves a LUT.
vec2 bsdf_env_brdf_approx(float NoV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4  r    = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

/// Kulla-Conty multiple-scattering compensation: single-scatter GGX drops light that bounces
/// between microfacets — over a third of a rough conductor's reflectance — so the lobe is scaled
/// by the reciprocal of its directional albedo.
vec3 bsdf_energy_compensation(vec3 f0, float NoV, float roughness) {
    vec2  ab  = bsdf_env_brdf_approx(NoV, roughness);
    float Ess = ab.x + ab.y;
    return vec3(1.0) + f0 * (1.0 / max(Ess, 1e-3) - 1.0);
}

/// f_r(V, L) and the combined sampling pdf for an explicitly chosen direction. A delta lobe
/// cannot be hit that way: a pure mirror returns 0, a smooth dielectric only its diffuse base.
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

/// Reflection lobe of a rough dielectric for an explicitly chosen direction. Refraction gets
/// zero density: connecting a light through the interface is a constrained-path problem left to
/// BSDF sampling. F is in the pdf too, since the sampler's reflect/refract branch is a
/// Fresnel-weighted coin flip.
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

/// Cosine-weighted direction about N, by Malley's method (polar mapping of the disc, lifted to
/// the hemisphere): a smooth warp of the unit square, so stratified input stays stratified.
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

    // One-sample MIS: weight by the combined pdf of both lobes, not the one drawn from, which
    // fireflies where they overlap.
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
/// Rough Surfaces". With H drawn from the visible normals and a Fresnel-weighted reflect/refract
/// coin flip, D, F and the refraction Jacobian cancel and the weight is G2/G1. A smooth
/// interface (`bsdf_is_delta`) takes H = N.
///
/// @param eta             Relative IOR being crossed into: 1/ior entering, ior leaving.
/// @param out_transmitted true when the ray passed through rather than bouncing off.
/// @param out_pdf         solid-angle density of a *reflected* sample, for MIS against NEE on
///                        that lobe; 0 when refracted or smooth, where nothing competes.
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
        // Near grazing, refract() can report TIR where the Fresnel test did not and return
        // vec3(0), which makes inv_dir NaN in traversal. Treat it as TIR.
        if (dot(out_L, out_L) < 1e-12) {
            out_L = reflect(-V, H);
        } else {
            out_transmitted = true;
            // Absorption depends on the distance travelled inside, so shade_transmissive applies
            // it when the ray reaches the far side.
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
