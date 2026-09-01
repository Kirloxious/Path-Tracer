#ifndef SHADE_SURFACE_GLSL
#define SHADE_SURFACE_GLSL

#include "path_state.glsl"
#include "shadow_state.glsl"
#include "queue.glsl"
#include "scene_buffers.glsl"
#include "rng.glsl"
#include "uniform_locations.glsl"
#include "clamp.glsl"
#include "lights.glsl"
#include "restir_common.glsl"
#include "geom.glsl"
#include "bsdf.glsl"
#include "path_continue.glsl"

// The whole shading step for one reflective surface vertex: direct lighting (ReSTIR at the
// anchor, analytic NEE elsewhere), the BSDF continuation, MIS bookkeeping and Russian
// roulette. Both shade_lambertian.comp and shade_metal.comp are thin wrappers over this —
// with a unified metallic-roughness BSDF the two kernels differ only in which queue they
// drain, and duplicating the body would guarantee the copies drift apart.
//
// Transmission is deliberately NOT handled here; shade_transmissive.comp keeps its own
// smooth reflect/refract path.
//
// The including kernel must declare the reservoir buffer before including this header:
//   layout(std430, binding = 18) restrict readonly buffer RestirReservoirsCurrent { Reservoir reservoirs[]; };

layout(location = LOC_NUM_LIGHT_GROUPS) uniform int num_light_groups;

vec2 shade_sample_triangle_bary(inout uint state) {
    float r1 = random_unilateral(state);
    float r2 = random_unilateral(state);
    float u  = sqrt(r1);
    return vec2(u * (1.0 - r2), u * r2);
}

void shade_surface(uint pid) {
    PathState s   = states[pid];
    Reservoir res = reservoirs[pid];
    uint      rng = s.rng_state;

    Material mat = mats[s.hit_matid];
    vec3     P   = s.hit_point;
    vec3     N   = s.hit_normal;
    vec3     V   = normalize(-s.ray_dir);

    // A delta (perfect-mirror) lobe cannot be sampled by an explicit light direction, so
    // every NEE technique is skipped on it and the path relies on hitting the emitter.
    bool delta = bsdf_is_delta(mat);

    bool nee_setup = false;

    // This vertex owns the pixel's reservoir when every bounce behind it was a perfect
    // mirror — exactly the chain restir_initial walked — *and* this surface is one
    // restir_initial would have stopped at. The second half is essential and not redundant:
    // the walk passes straight through delta mirrors, so on such a surface the reservoir
    // describes a different vertex further down the chain, and consuming it here would light
    // this surface with another surface's direct lighting.
    bool at_restir_anchor = (s.flags & FLAG_SPECULAR_PREFIX) != 0u && restir_can_anchor(mat);
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

        // Visibility is already folded into res.W (initial/temporal/spatial each shadow-test
        // the chosen sample at the receiver's surface and zero W on occlusion), so this skips
        // shadow_queue_push and the redundant trace_shadow ray.
        // res.W is an estimator weight, not authored radiance — it spikes when the chosen
        // sample had a tiny target_pdf, so it takes the indirect clamp.
        s.radiance += clamp_indirect(s.throughput * f * lmat.emission * cos_theta * res.W);
        // Leave nee_setup = false so this path doesn't enter shadow_queue.
    }
    else if (at_restir_anchor) {
        // ReSTIR was the chosen NEE technique for this vertex but produced no usable sample
        // (empty reservoir or W=0). Don't fall back to inline NEE — it would double-sample
        // relative to the light pdf ReSTIR is already approximating, and the ReSTIR path
        // tested visibility, so a zero contribution is correct.
    }
    else if (num_light_groups > 0 && !delta) {
        int        gi  = sample_light_group(num_light_groups, rng);
        LightGroup grp = light_groups[gi];

        int      lo   = sample_light_triangle(grp, rng);
        Triangle tri  = triangles[lo];
        vec2     bary = shade_sample_triangle_bary(rng);
        vec3     v0   = vertices[tri.indices.x].position;
        vec3     sp   = v0 + bary.x * tri.e1 + bary.y * tri.e2;

        vec3  d        = sp - P;
        float dist2    = dot(d, d);
        float inv_dist = inversesqrt(dist2);
        vec3  L        = d * inv_dist;
        float dist     = dist2 * inv_dist;

        // Solid-angle pdf, treating the whole group as one uniform area light.
        float cross_dot = dot(cross(tri.e1, tri.e2), L);
        if (cross_dot < 0.0 && grp.total_area > 0.0) {
            float light_pdf_combined = -2.0 * tri.area * dist2 / (cross_dot * grp.total_area * float(num_light_groups));
            float cos_theta          = max(0.0, dot(N, L));
            if (cos_theta > 0.0 && light_pdf_combined > 0.0) {
                // f and the BSDF pdf for this same direction come from one evaluation, so the
                // MIS balance heuristic below can never disagree with the BSDF it is weighting.
                float bsdf_pdf_for_nee;
                vec3  f = bsdf_eval(mat, N, V, L, bsdf_pdf_for_nee);

                if (dot(f, f) > 0.0) {
                    Material lmat       = mats[tri.material_index];
                    float    mis_weight = light_pdf_combined / (light_pdf_combined + bsdf_pdf_for_nee);
                    vec3     contrib    = clamp_indirect(s.throughput * f * lmat.emission * cos_theta * mis_weight / light_pdf_combined);

                    shadow_states[pid].nee_dir  = L;
                    shadow_states[pid].nee_dist = dist;
                    shadow_states[pid].nee_le   = contrib;
                    nee_setup                   = true;
                }
            }
        }
    }

    if (nee_setup) {
        shadow_queue_push(pid);
    }

    // ---- BSDF continuation ----
    vec3  scatter_dir;
    vec3  weight;
    float pdf;
    bool  sampled_delta;
    bool  alive = bsdf_sample(mat, N, V, rng, scatter_dir, weight, pdf, sampled_delta);

    if (alive) {
        // Store the BSDF pdf of the continuation so the next emissive hit can MIS-weight Le.
        // A delta lobe has no density to weight against; path_continue() handles its flags.
        s.pdf_bsdf = sampled_delta ? 0.0 : pdf;
        s.throughput *= weight;

        // Which NEE technique ran here, so the next emissive hit knows how to weight itself:
        // - use_restir: ReSTIR covered direct lighting from this vertex, and the next emissive
        //   hit must skip its contribution entirely.
        // - nee_setup:  analytic NEE was scheduled; the next emissive hit balance-MIS-es
        //   against the analytic light pdf.
        // - Neither: no competing estimator ran, so the next emissive hit takes weight 1.
        uint nee_flags = use_restir ? FLAG_RESTIR_HANDLED : (nee_setup ? FLAG_PREV_NON_SPECULAR : 0u);
        alive = path_continue(s, rng, scatter_dir, false, sampled_delta, nee_flags);
    }

    path_commit(pid, s, rng, alive);
}

#endif
