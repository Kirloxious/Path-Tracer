#ifndef BSDF_GLSL
#define BSDF_GLSL

#include "primitives.glsl"
#include "math.glsl"
#include "rng.glsl"

// Metallic-roughness BSDF (Lambert + GGX), mirroring material.h. V points toward the viewer, L toward
// the light; `roughness` is perceptual (alpha = roughness^2).

// Below this alpha the lobe is a delta. restir_initial's reflect() walk and the shade kernels must
// agree exactly on which surfaces are mirrors.
const float BSDF_DELTA_ALPHA = 1e-3;

float bsdf_alpha(Material m) {
    return m.roughness * m.roughness;
}

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

/// Delta specular and *no* diffuse lobe. Unlike bsdf_is_delta, only a pure mirror is invisible to NEE
/// and a deterministic scatter restir_initial's walk can follow.
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

/// eta = ior_from / ior_to. Returns 1 under total internal reflection.
float bsdf_fresnel_dielectric(float cosine, float eta) {
    cosine = clamp(cosine, 0.0, 1.0);
    // Leaving the denser medium Schlick must use the transmitted cosine, or F jumps from r0 to 1 at the
    // critical angle.
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

/// Branchless ONB (Duff et al. 2017).
void bsdf_onb(vec3 n, out vec3 t, out vec3 b) {
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float c = n.x * n.y * a;
    t = vec3(1.0 + s * n.x * n.x * a, s * c, -s * n.x);
    b = vec3(c, s + n.y * n.y * a, -n.y);
}

/// Weights each lobe by its reflectance at this angle, so grazing samples aren't wasted on a vanished
/// diffuse lobe. bsdf_eval and bsdf_sample must pass the same NoV.
float bsdf_spec_prob(Material m, float NoV) {
    float ws = luminance(bsdf_F_schlick(bsdf_f0(m), NoV));
    float wd = luminance(bsdf_diffuse_albedo(m)) * (1.0 - ws);
    return ws / max(wd + ws, 1e-6);
}

/// Karis's analytic fit to the split-sum DFG integral (SIGGRAPH 2014); saves a LUT.
vec2 bsdf_env_brdf_approx(float NoV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4  r    = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

/// Kulla-Conty compensation: single-scatter GGX loses over a third of a rough conductor's reflectance.
vec3 bsdf_energy_compensation(vec3 f0, float NoV, float roughness) {
    vec2  ab  = bsdf_env_brdf_approx(NoV, roughness);
    float Ess = ab.x + ab.y;
    return vec3(1.0) + f0 * (1.0 / max(Ess, 1e-3) - 1.0);
}

/// A delta lobe can't be hit by an explicit direction: a pure mirror returns 0, a smooth dielectric
/// only its diffuse base.
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
    // (1 - F): what the specular lobe reflects cannot also enter the diffuse one.
    vec3 diffuse = (vec3(1.0) - F) * bsdf_diffuse_albedo(m) * (1.0 / PI);

    float ps = bsdf_spec_prob(m, NoV);
    pdf = ps * (D * bsdf_G1_smith(NoV, alpha) / (4.0 * NoV)) + (1.0 - ps) * (NoL / PI);

    return diffuse + specular;
}

/// Refraction gets zero density: light connection through the interface is left to BSDF sampling. F
/// is in the pdf because the sampler's reflect/refract branch is a Fresnel coin flip.
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

/// Malley's method: a smooth warp of the unit square, so stratified input stays stratified.
vec3 bsdf_sample_cosine(vec3 N, vec2 u) {
    float r   = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    vec3  T, B;
    bsdf_onb(N, T, B);
    return normalize(r * cos(phi) * T + r * sin(phi) * B + sqrt(max(0.0, 1.0 - u.x)) * N);
}

/// VNDF sampling (Heitz 2018). `Ve` and the result are in the local frame where N = +z.
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

/// out_weight = f * cos / pdf; out_pdf is 0 for a delta lobe. Returns false if the path should die.
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
        // A pure mirror skips the coin flip so its scatter is exactly the reflect() restir_initial reproduces.
        float ps = bsdf_is_mirror(m) ? 1.0 : bsdf_spec_prob(m, NoV);
        if (ps >= 1.0 || sampler_1d(smp) < ps) {
            out_L = reflect(-V, N);
            if (dot(out_L, N) <= 0.0) {
                return false;
            }
            // D and G cancel against the pdf, leaving only Fresnel.
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

    // One-sample MIS: weight by the combined pdf of both lobes, not the one drawn from.
    vec3 f = bsdf_eval(m, N, V, out_L, out_pdf);
    if (out_pdf <= 0.0) {
        return false;
    }
    out_weight = f * NoL / out_pdf;
    return true;
}

/// Walter et al. 2007: with VNDF H and a Fresnel coin flip, D, F and the Jacobian cancel to G2/G1.
/// out_pdf is the density of a *reflected* rough sample, for MIS against NEE; 0 otherwise.
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
            // Must match bsdf_eval_dielectric_reflection's pdf exactly.
            out_pdf = F * bsdf_D_ggx(max(dot(N, H), 0.0), alpha) * bsdf_G1_smith(NoV, alpha) / (4.0 * NoV);
        }
    } else {
        out_L = refract(-V, H, eta);
        // Near grazing refract() can report TIR where Fresnel didn't, returning vec3(0) (NaN inv_dir).
        if (dot(out_L, out_L) < 1e-12) {
            out_L = reflect(-V, H);
        } else {
            out_transmitted = true;
            // Absorption depends on distance travelled, so shade_transmissive applies it at the far side.
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
