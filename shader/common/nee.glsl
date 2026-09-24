#ifndef NEE_GLSL
#define NEE_GLSL

#include "scene_buffers.glsl"
#include "shadow_state.glsl"
#include "rng.glsl"
#include "clamp.glsl"
#include "lights.glsl"
#include "bsdf.glsl"
#include "envmap.glsl"

// Analytic next-event estimation for shade_surface and shade_transmissive, which differ only in
// the lobe evaluated — one copy of the MIS bookkeeping, so the two cannot drift apart.

// The surface an estimator is standing on. `dielectric` picks the lobe: a rough dielectric's
// reflection instead of the opaque metallic-roughness BSDF.
struct NeeSurface {
    vec3  P;
    vec3  N;
    vec3  V;
    uint  matid;
    float eta;        // relative IOR being crossed; read only when `dielectric`
    bool  dielectric;
};

vec3 nee_eval(in NeeSurface surf, vec3 L, out float pdf) {
    Material m = mats[surf.matid];
    if (surf.dielectric) {
        return bsdf_eval_dielectric_reflection(m, surf.N, surf.V, L, surf.eta, pdf);
    }
    return bsdf_eval(m, surf.N, surf.V, L, pdf);
}

/**
 * Schedules up to two independent shadow rays for this vertex, toward an area light and toward
 * the environment.
 *
 * @param allow_light_nee false at a ReSTIR anchor, where resampling already estimates the same
 *                        area-light integral and a second estimator would double-count it.
 * @param dim_base        first sampler dimension group this vertex owns; uses [base, base+5).
 * @param ss              fully written, including the two validity flags — the caller must not
 *                        assume anything survives from a previous bounce.
 * @return the FLAG_* bits describing which techniques ran, for the next vertex's MIS. A
 *         technique that ran but drew an unusable sample still counts: the BSDF side's balance
 *         weight must apply whenever the technique *could* have produced the direction, or the
 *         two weights stop summing to one.
 */
uint nee_schedule(in NeeSurface surf, in vec3 throughput, bool allow_light_nee, int num_light_groups, inout Sampler smp, uint dim_base,
                  out ShadowState ss) {
    ss.nee_dir   = vec3(0.0);
    ss.nee_dist  = 0.0;
    ss.nee_le    = vec3(0.0);
    ss.nee_valid = 0.0;
    ss.env_dir   = vec3(0.0);
    ss.env_valid = 0.0;
    ss.env_le    = vec3(0.0);
    ss.nee_tri   = 0xFFFFFFFFu;

    // A delta lobe has zero density for any chosen direction, so no NEE technique can reach it.
    // A smooth opaque dielectric still has its diffuse base; smooth glass has nothing.
    Material m = mats[surf.matid];
    if (surf.dielectric ? bsdf_is_delta(m) : bsdf_is_mirror(m)) {
        return 0u;
    }

    uint flags = 0u;

    if (allow_light_nee && num_light_groups > 0) {
        flags |= FLAG_PREV_NON_SPECULAR;
        sampler_set_dim(smp, dim_base);
        int        gi  = sample_light_group(num_light_groups, smp);
        LightGroup grp = light_groups[gi];

        int      lo  = sample_light_triangle(grp, smp);
        Triangle tri = triangles[lo];
        vec3     sp  = light_point(tri, sample_triangle_bary(smp));

        vec3  d        = sp - surf.P;
        float dist2    = dot(d, d);
        float inv_dist = inversesqrt(dist2);
        vec3  L        = d * inv_dist;
        float dist     = dist2 * inv_dist;

        float light_pdf = light_solid_angle_pdf(tri, grp, L, dist2);
        float cos_theta = max(0.0, dot(surf.N, L));
        if (cos_theta > 0.0 && light_pdf > 0.0) {
            // f and the BSDF pdf for this same direction come from one evaluation, so the
            // balance heuristic can never disagree with the BSDF it is weighting.
            float bsdf_pdf;
            vec3  f = nee_eval(surf, L, bsdf_pdf);
            if (dot(f, f) > 0.0) {
                Material lmat       = mats[tri.material_index];
                float    mis_weight = light_pdf / (light_pdf + bsdf_pdf);

                ss.nee_dir   = L;
                ss.nee_dist  = dist;
                ss.nee_le    = clamp_indirect(throughput * f * material_emission(lmat) * cos_theta * mis_weight / light_pdf);
                ss.nee_valid = 1.0;
                ss.nee_tri   = uint(lo);
            }
        }
    }

    // Runs at a ReSTIR anchor too: ReSTIR resamples emissive triangles only, so without this
    // an HDR sky never lights a primary surface by anything but a lucky escaping ray.
    if (env_map_valid != 0) {
        flags |= FLAG_PREV_ENV_NEE;
        sampler_set_dim(smp, dim_base + 3u);

        vec3  env_le;
        float env_pdf;
        vec3  L = envmap_sample(sampler_2d(smp), sampler_2d(smp), env_le, env_pdf);

        float cos_theta = max(0.0, dot(surf.N, L));
        if (env_pdf > 0.0 && cos_theta > 0.0 && dot(env_le, env_le) > 0.0) {
            float bsdf_pdf;
            vec3  f = nee_eval(surf, L, bsdf_pdf);
            if (dot(f, f) > 0.0) {
                float mis_weight = env_pdf / (env_pdf + bsdf_pdf);

                ss.env_dir   = L;
                ss.env_le    = clamp_indirect(throughput * f * env_le * cos_theta * mis_weight / env_pdf);
                ss.env_valid = 1.0;
            }
        }
    }

    return flags;
}

#endif
