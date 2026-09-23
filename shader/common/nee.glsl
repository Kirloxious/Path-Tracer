#ifndef NEE_GLSL
#define NEE_GLSL

#include "scene_buffers.glsl"
#include "shadow_state.glsl"
#include "rng.glsl"
#include "clamp.glsl"
#include "lights.glsl"
#include "bsdf.glsl"
#include "envmap.glsl"

// Analytic next-event estimation, shared by every kernel that shades a surface.
//
// Owned here rather than inlined into shade_surface because shade_transmissive needs the same
// estimator against a different lobe. The two differ only in which BSDF is evaluated; the
// light sampling, the environment sampling and — the part that actually rots when copied — the
// MIS bookkeeping are identical, and two copies of a balance heuristic drift silently.

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

vec2 nee_triangle_bary(inout Sampler smp) {
    vec2  u = sampler_2d(smp);
    float r = sqrt(u.x);
    return vec2(r * (1.0 - u.y), r * u.y);
}

/**
 * Schedules up to two shadow rays for this vertex: one toward an area light, one toward the
 * environment.
 *
 * The two are independent rather than alternatives. A direction drawn toward an emissive
 * triangle terminates on it and one drawn toward the sky escapes the scene, so neither
 * technique can produce the other's samples and running both double-counts nothing.
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
    ss._pad      = 0.0;

    // A delta lobe has zero density for any explicitly chosen direction, so no NEE technique
    // can reach it and the path has to find lights by scattering into them. A smooth opaque
    // dielectric still has its diffuse base to light; smooth glass has nothing.
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

        int      lo   = sample_light_triangle(grp, smp);
        Triangle tri  = triangles[lo];
        vec2     bary = nee_triangle_bary(smp);
        vec3     v0   = vertices[tri.indices.x].position;
        vec3     sp   = v0 + bary.x * tri.e1 + bary.y * tri.e2;

        vec3  d        = sp - surf.P;
        float dist2    = dot(d, d);
        float inv_dist = inversesqrt(dist2);
        vec3  L        = d * inv_dist;
        float dist     = dist2 * inv_dist;

        // Solid-angle pdf, treating the whole group as one uniform area light.
        float cross_dot = dot(cross(tri.e1, tri.e2), L);
        if (cross_dot < 0.0 && grp.total_area > 0.0) {
            float light_pdf = grp.select_pdf * -2.0 * tri.area * dist2 / (cross_dot * grp.total_area);
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
                }
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
