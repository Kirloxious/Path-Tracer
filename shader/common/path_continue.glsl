#ifndef PATH_CONTINUE_GLSL
#define PATH_CONTINUE_GLSL

#include "path_state.glsl"
#include "queue.glsl"
#include "rng.glsl"
#include "geom.glsl"
#include "uniform_locations.glsl"
#include "constants.glsl"

// Shared so every shade kernel continues paths alike.
layout(location = LOC_BOUNCE_INDEX) uniform int bounce_index;

Sampler path_sampler(in PathState s) {
    return sampler_init(s.rng_state, frame_index, s.bounce * SAMPLER_DIMS_PER_BOUNCE);
}

// `throughput` must already carry this scatter's weight. keep_specular_prefix only for a perfect mirror,
// the one scatter restir_initial can reproduce. Returns false when roulette kills the path.
bool path_continue(inout PathState s, inout Sampler smp, vec3 new_dir, bool keep_specular_prefix, uint nee_flags) {
    // Before the bounce increments, so roulette's dimension doesn't depend on how many the BSDF drew.
    uint rr_dim  = s.bounce * SAMPLER_DIMS_PER_BOUNCE + 11u;
    s.ray_origin = surface_ray_origin(s.hit_point, s.hit_triangle_idx, s.hit_normal, new_dir);
    s.ray_dir    = new_dir;
    s.bounce++;

    s.flags &= ~(FLAG_PREV_NON_SPECULAR | FLAG_RESTIR_HANDLED | FLAG_PREV_ENV_NEE);
    if (!keep_specular_prefix) {
        s.flags &= ~FLAG_SPECULAR_PREFIX;
    }
    s.flags |= nee_flags;

    // Capped: throughput grown past 1 by earlier roulette divisions would otherwise never terminate.
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

/// Must run for a dead path too: the state carries this vertex's radiance.
void path_commit(uint pid, inout PathState s, bool alive) {
    states[pid] = s;

    if (alive && bounce_index + 1 < max_bounces) {
        ray_queue_push(pid);
    }
}

#endif
