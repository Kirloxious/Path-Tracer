#ifndef PATH_CONTINUE_GLSL
#define PATH_CONTINUE_GLSL

#include "path_state.glsl"
#include "queue.glsl"
#include "rng.glsl"
#include "geom.glsl"
#include "uniform_locations.glsl"
#include "constants.glsl"

// Everything a scattered path does *after* its new direction has been chosen: ray-origin
// offset, bounce accounting, MIS flag bookkeeping, Russian roulette, state store and requeue.
//
// Owned here rather than copied into each shade kernel. shade_opaque and shade_transmissive
// scatter by completely different rules but continue the path by identical ones, and the
// duplicated copies had already started to diverge in their comments — the kind of drift that
// ends with two subtly different Russian-roulette cutoffs.
//
// Declared here, not in the kernels, so every shade kernel that continues a path agrees on it.
layout(location = LOC_BOUNCE_INDEX) uniform int bounce_index;

// The sampler for one path vertex. Rebasing the dimension on `bounce` is what keeps the
// vertices of a path drawing from disjoint groups instead of re-walking the same ones.
Sampler path_sampler(in PathState s) {
    return sampler_init(s.rng_state, frame_index, s.bounce * SAMPLER_DIMS_PER_BOUNCE);
}

/**
 * Advances the path one bounce.
 *
 * @param s                    Path state, mutated in place. `throughput` must already carry
 *                             this scatter's weight; the caller owns the BSDF.
 * @param smp                  Sample stream, drawn from by Russian roulette.
 * @param new_dir              The scattered direction. The origin is offset to whichever side
 *                             it leaves on, so refraction needs no flag of its own.
 * @param keep_specular_prefix true only for a perfect-mirror scatter — the one scatter
 *                             restir_initial's deterministic reflect() walk can reproduce.
 *                             Any other lobe ends the chain.
 * @param nee_flags            Which NEE techniques ran at this vertex, so the next emissive
 *                             hit or environment miss knows how to weight itself: one of
 *                             FLAG_RESTIR_HANDLED / FLAG_PREV_NON_SPECULAR / neither for area
 *                             lights, optionally OR-ed with FLAG_PREV_ENV_NEE.
 * @return false when Russian roulette killed the path.
 */
bool path_continue(inout PathState s, inout Sampler smp, vec3 new_dir, bool keep_specular_prefix, uint nee_flags) {
    // Pinned before the bounce is incremented, so roulette draws from the same group whether
    // or not the BSDF above happened to be a delta lobe that took no samples at all.
    uint rr_dim  = s.bounce * SAMPLER_DIMS_PER_BOUNCE + 11u;
    s.ray_origin = surface_ray_origin(s.hit_point, s.hit_triangle_idx, s.hit_normal, new_dir);
    s.ray_dir    = new_dir;
    s.bounce++;

    s.flags &= ~(FLAG_PREV_NON_SPECULAR | FLAG_RESTIR_HANDLED | FLAG_PREV_ENV_NEE);
    if (!keep_specular_prefix) {
        s.flags &= ~FLAG_SPECULAR_PREFIX;
    }
    s.flags |= nee_flags;

    // Russian roulette after a few bounces. Survival probability is capped at 0.95:
    // uncapped, a throughput grown past 1 through earlier roulette divisions gives p > 1,
    // the draw can then never exceed p, and roulette stops terminating anything —
    // every path runs to max_bounces. The `p <= 0` early kill keeps a fully absorbed path
    // dying immediately.
    if (s.bounce > 2) {
        float p = min(max(s.throughput.r, max(s.throughput.g, s.throughput.b)), 0.95);
        sampler_set_dim(smp, rr_dim);
        if (p <= 0.0 || sampler_1d(smp) > p) {
            return false;
        }
        s.throughput /= p;
    }
    return true;
}

/// Writes the path state back and requeues it for tracing if it is still alive and the bounce
/// budget allows. Must run even for a dead path — it carries this vertex's accumulated radiance.
void path_commit(uint pid, inout PathState s, bool alive) {
    states[pid] = s;

    if (alive && bounce_index + 1 < max_bounces) {
        ray_queue_push(pid);
    }
}

#endif
