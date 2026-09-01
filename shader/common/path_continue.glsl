#ifndef PATH_CONTINUE_GLSL
#define PATH_CONTINUE_GLSL

#include "path_state.glsl"
#include "queue.glsl"
#include "rng.glsl"
#include "geom.glsl"
#include "uniform_locations.glsl"

// Everything a scattered path does *after* its new direction has been chosen: ray-origin
// offset, bounce accounting, MIS flag bookkeeping, Russian roulette, state store and requeue.
//
// Owned here rather than copied into each shade kernel. shade_opaque and shade_transmissive
// scatter by completely different rules but continue the path by identical ones, and the
// duplicated copies had already started to diverge in their comments — the kind of drift that
// ends with two subtly different Russian-roulette cutoffs.
//
// These uniforms are declared here, not in the kernels, so every shade kernel that continues
// a path necessarily agrees on the bounce budget.
layout(location = LOC_BOUNCE_INDEX) uniform int bounce_index;
layout(location = LOC_MAX_BOUNCES) uniform int max_bounces;

/**
 * Advances the path one bounce.
 *
 * @param s                    Path state, mutated in place. `throughput` must already carry
 *                             this scatter's weight; the caller owns the BSDF.
 * @param rng                  RNG state, advanced by Russian roulette.
 * @param new_dir              The scattered direction.
 * @param crossed_surface      true when the scatter passed *through* the surface (refraction)
 *                             rather than bouncing off it, which flips the origin offset to
 *                             the far side.
 * @param keep_specular_prefix true only for a perfect-mirror scatter — the one scatter
 *                             restir_initial's deterministic reflect() walk can reproduce.
 *                             Any other lobe ends the chain.
 * @param nee_flags            FLAG_RESTIR_HANDLED, FLAG_PREV_NON_SPECULAR, or 0 — which NEE
 *                             technique (if any) ran at this vertex, so the next emissive hit
 *                             knows how to weight itself.
 * @return false when Russian roulette killed the path.
 */
bool path_continue(inout PathState s, inout uint rng, vec3 new_dir, bool crossed_surface, bool keep_specular_prefix, uint nee_flags) {
    // Offset along the *geometric* normal: `hit_normal` is the shading normal, which on
    // curved surfaces near silhouettes can point past the tangent plane and shove the origin
    // inside the primitive (the dragon's firefly noise). At bounce 0 hit_triangle_idx isn't
    // seeded — generate.comp builds the state from the gbuffer, which carries only the
    // shading normal — so fall back to it there.
    bool front_face   = (s.flags & FLAG_FRONT_FACE) != 0u;
    vec3 offset_basis = (s.bounce > 0u) ? triangle_geom_normal(s.hit_triangle_idx, front_face) : s.hit_normal;
    //   reflected/scattered: ray stays on the side it came from → offset along +n
    //   refracted:           ray crosses into the other side    → offset along -n
    s.ray_origin = s.hit_point + 0.001 * (crossed_surface ? -offset_basis : offset_basis);
    s.ray_dir    = new_dir;
    s.bounce++;

    s.flags &= ~(FLAG_PREV_NON_SPECULAR | FLAG_RESTIR_HANDLED);
    if (!keep_specular_prefix) {
        s.flags &= ~FLAG_SPECULAR_PREFIX;
    }
    s.flags |= nee_flags;

    // Russian roulette after a few bounces. Survival probability is capped at 0.95:
    // uncapped, a throughput grown past 1 through earlier roulette divisions gives p > 1,
    // `random_unilateral() > p` can then never fire, and roulette stops terminating anything —
    // every path runs to max_bounces. The `p <= 0` early kill keeps a fully absorbed path
    // dying immediately.
    if (s.bounce > 2) {
        float p = min(max(s.throughput.r, max(s.throughput.g, s.throughput.b)), 0.95);
        if (p <= 0.0 || random_unilateral(rng) > p) {
            return false;
        }
        s.throughput /= p;
    }
    return true;
}

/// Writes the path state back and requeues it for tracing if it is still alive and the bounce
/// budget allows. Must run even for a dead path — it carries this vertex's accumulated radiance.
void path_commit(uint pid, inout PathState s, uint rng, bool alive) {
    s.rng_state = rng;
    states[pid] = s;

    if (alive && bounce_index + 1 < max_bounces) {
        ray_queue_push(pid);
    }
}

#endif
