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

// The whole shading step for one reflective surface vertex: direct lighting (ReSTIR at the
// anchor, analytic NEE elsewhere), the BSDF continuation, MIS bookkeeping and Russian
// roulette. shade_opaque.comp is a thin wrapper over this, for Diffuse and Specular alike.
//
// Transmission is deliberately NOT handled here; shade_transmissive.comp keeps its own
// smooth reflect/refract path.
//
// The including kernel must declare the reservoir buffer before including this header:
//   layout(std430, binding = BIND_RESERVOIRS_CURRENT) restrict readonly buffer RestirReservoirsCurrent { Reservoir reservoirs[]; };

void shade_surface(uint pid) {
    PathState s   = states[pid];
    Reservoir res = reservoirs[pid];
    Sampler   smp = path_sampler(s);

    Material mat = mats[s.hit_matid];
    vec3     P   = s.hit_point;
    vec3     N   = s.hit_normal;
    vec3     V   = normalize(-s.ray_dir);

    // This vertex owns the pixel's reservoir when every bounce behind it was a perfect
    // mirror — exactly the chain restir_initial walked — *and* this surface is one
    // restir_initial would have stopped at. The second half is essential and not redundant:
    // the walk passes straight through delta mirrors, so on such a surface the reservoir
    // describes a different vertex further down the chain, and consuming it here would light
    // this surface with another surface's direct lighting.
    //
    // M > 0 confirms restir_initial actually built a reservoir for this vertex; it writes M = 0
    // when the walk gave up (sky, no lights, a mirror chain past MAX_MIRROR_DEPTH), and such a
    // vertex falls back to analytic NEE.
    bool at_restir_anchor = (s.flags & FLAG_SPECULAR_PREFIX) != 0u && restir_can_anchor(mat) && res.M > 0.0;
    bool use_restir       = at_restir_anchor && (res.light_tri_idx != RESTIR_INVALID_TRI) && (res.W > 0.0);

    if (use_restir) {
        Triangle tri  = triangles[res.light_tri_idx];
        Material lmat = mats[tri.material_index];

        vec3 v0 = vertices[tri.indices.x].position;
        vec3 sp = v0 + res.bary.x * tri.e1 + res.bary.y * tri.e2;
        vec3 L  = normalize(sp - P);

        float cos_theta = max(0.0, dot(N, L));

        // The full BRDF, matching the target pdf restir_* now resamples against.
        float ignored_pdf;
        vec3  f = bsdf_eval(mat, N, V, L, ignored_pdf);

        // Visibility is already folded into res.W (each ReSTIR pass shadow-tests the chosen
        // sample at the receiver's surface and zeroes W on occlusion), so this schedules no
        // shadow ray of its own.
        //
        // Unclamped: use_restir requires FLAG_SPECULAR_PREFIX, so every vertex behind this one
        // was a perfect mirror and this contribution is directly visible up to Fresnel — the
        // same exemption shade_emissive gives a primary hit on an emitter.
        s.radiance += s.throughput * f * material_emission(lmat) * cos_theta * res.W;
    }

    // Analytic NEE everywhere except at a ReSTIR anchor, whose reservoir already estimates the
    // same area-light integral; a second estimator there would double-count it, and an anchor
    // whose reservoir came up empty is a *correct* zero, not a case to fall back from. The
    // environment half of nee_schedule runs either way — see its note on why the two
    // techniques are independent rather than alternatives.
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

    // ---- BSDF continuation ----
    // Pinned past the NEE budget so the continuation direction draws from the same dimensions
    // regardless of which of the three direct-lighting branches above ran — otherwise a pixel
    // that switches branch between frames restarts its sequence and loses stratification.
    sampler_set_dim(smp, s.bounce * SAMPLER_DIMS_PER_BOUNCE + 6u);

    vec3  scatter_dir;
    vec3  weight;
    float pdf;
    bool  sampled_delta;
    bool  alive = bsdf_sample(mat, N, V, smp, scatter_dir, weight, pdf, sampled_delta);

    if (alive) {
        // Store the BSDF pdf of the continuation so the next emissive hit can MIS-weight Le.
        // A delta lobe has no density to weight against; path_continue() handles its flags.
        s.pdf_bsdf = sampled_delta ? 0.0 : pdf;
        s.throughput *= weight;

        // FLAG_RESTIR_HANDLED makes the next emissive hit drop its contribution entirely: the
        // reservoir is this vertex's whole area-light estimator, and W == 0 is part of it, not
        // a gap for the BSDF sample to fill. It cannot collide with FLAG_PREV_NON_SPECULAR, since
        // at_restir_anchor is exactly what suppressed the analytic light sample above.
        //
        // A delta sample carries no flags: neither NEE nor the reservoir can produce a direction
        // on a delta lobe, so whatever that scatter finds is its own and takes full weight. Only
        // a pure mirror keeps the specular prefix — a smooth dielectric's coat was a coin flip.
        uint cont_flags = sampled_delta ? 0u : (nee_flags | (at_restir_anchor ? FLAG_RESTIR_HANDLED : 0u));
        alive = path_continue(s, smp, scatter_dir, sampled_delta && bsdf_is_mirror(mat), cont_flags);
    }

    path_commit(pid, s, alive);
}

#endif
