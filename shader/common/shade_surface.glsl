#ifndef SHADE_SURFACE_GLSL
#define SHADE_SURFACE_GLSL

#include "path_state.glsl"
#include "shadow_state.glsl"
#include "queue.glsl"
#include "scene_buffers.glsl"
#include "rng.glsl"
#include "clamp.glsl"
#include "lights.glsl"
#include "restir_common.glsl"
#include "geom.glsl"
#include "bsdf.glsl"
#include "nee.glsl"
#include "path_continue.glsl"

layout(std430, binding = BIND_RESERVOIRS_CURRENT) restrict readonly buffer RestirReservoirsCurrent {
    Reservoir reservoirs[];
};

void shade_surface(uint pid) {
    PathState s   = states[pid];
    Reservoir res = reservoirs[pid];
    Sampler   smp = path_sampler(s);

    Material mat = mats[s.hit_matid];
    vec3     P   = s.hit_point;
    vec3     N   = s.hit_normal;
    vec3     V   = normalize(-s.ray_dir);

    // Owns the reservoir only behind an all-mirror prefix where restir_initial also stopped (its walk
    // passes through mirrors). M == 0 means the walk gave up; fall back to analytic NEE.
    bool at_restir_anchor = (s.flags & FLAG_SPECULAR_PREFIX) != 0u && restir_can_anchor(mat) && res.M > 0.0;
    bool use_restir       = at_restir_anchor && (res.light_tri_idx != RESTIR_INVALID_TRI) && (res.W > 0.0);

    if (use_restir) {
        Triangle tri  = triangles[res.light_tri_idx];
        Material lmat = mats[tri.material_index];

        vec3 L = normalize(light_point(tri, res.bary) - P);

        float cos_theta = max(0.0, dot(N, L));

        float ignored_pdf;
        vec3  f = bsdf_eval(mat, N, V, L, ignored_pdf);

        // Visibility is folded into res.W. Unclamped: behind this vertex are only mirrors, so it is direct lighting.
        s.radiance += s.throughput * f * material_emission(lmat) * cos_theta * res.W;
    }

    // The reservoir is the anchor's whole area-light estimator; an empty one is a correct zero.
    NeeSurface surf;
    surf.P          = P;
    surf.N          = N;
    surf.V          = V;
    surf.matid      = s.hit_matid;
    surf.eta        = 1.0;
    surf.dielectric = false;

    ShadowState ss;
    uint nee_flags = nee_schedule(surf, s.throughput, !at_restir_anchor, num_light_groups, smp, s.bounce * SAMPLER_DIMS_PER_BOUNCE, ss);

    if (ss.nee_valid != 0.0 || ss.env_valid != 0.0) {
        shadow_states[pid] = ss;
        shadow_queue_push(pid);
    }

    // Pinned past the NEE budget so the continuation draws the same dimensions whichever branch ran.
    sampler_set_dim(smp, s.bounce * SAMPLER_DIMS_PER_BOUNCE + 6u);

    vec3  scatter_dir;
    vec3  weight;
    float pdf;
    bool  sampled_delta;
    bool  alive = bsdf_sample(mat, N, V, smp, scatter_dir, weight, pdf, sampled_delta);

    if (alive) {
        s.pdf_bsdf = sampled_delta ? 0.0 : pdf;
        s.throughput *= weight;

        // A delta sample carries no NEE flags and takes full weight. Only a pure mirror keeps the specular
        // prefix: a smooth dielectric's coat was a coin flip restir_initial can't follow.
        uint cont_flags = sampled_delta ? 0u : (nee_flags | (at_restir_anchor ? FLAG_RESTIR_HANDLED : 0u));
        alive = path_continue(s, smp, scatter_dir, sampled_delta && bsdf_is_mirror(mat), cont_flags);
    }

    path_commit(pid, s, alive);
}

#endif
