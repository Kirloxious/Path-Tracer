#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#include "scene_buffers.glsl"
#include "rng.glsl"

// Area-proportional triangle selection within a light group, via the alias table baked into
// Triangle::alias_packed by World::buildLightGroups().
//
// Draw a uniform slot, accept it with its stored probability, otherwise take its alias. Two
// loads at worst, no data-dependent branching on loop count. The CDF binary search this
// replaced walked log2(count) dependent scattered loads per candidate — with 32 ReSTIR
// candidates per pixel against a 960-triangle sphere light, that was ~320 serialized memory
// round-trips per pixel before any shading happened.
//
// Returns an absolute index into TrianglesBuffer. The selection probability is exactly
// area / total_area, which is what the solid-angle pdf used by every caller assumes.
int sample_light_triangle(in LightGroup grp, inout Sampler smp) {
    // One 2D draw rather than two 1D ones. The alias method needs each coordinate uniform on
    // its own, which a Sobol' pair is; it does not need them independent.
    vec2 u    = sampler_2d(smp);
    int  slot = min(int(u.x * float(grp.count)), grp.count - 1);

    // `packed` is a reserved word in GLSL, hence the name.
    uint  entry  = triangles[grp.begin + slot].alias_packed;
    float accept = float(entry >> 16) * (1.0 / 65535.0);
    if (u.y >= accept) {
        slot = int(entry & 0xFFFFu);
    }
    return grp.begin + slot;
}

// Power-weighted pick over light groups, by the same alias scheme as the triangles above.
// The chosen group's `select_pdf` is the probability of this draw; callers multiply their
// within-group pdf by it rather than dividing by the group count, which is what a uniform
// pick would have meant.
int sample_light_group(in int num_light_groups, inout Sampler smp) {
    vec2 u  = sampler_2d(smp);
    int  gi = min(int(u.x * float(num_light_groups)), num_light_groups - 1);

    uint  entry  = light_groups[gi].alias_packed;
    float accept = float(entry >> 16) * (1.0 / 65535.0);
    return (u.y >= accept) ? int(entry & 0xFFFFu) : gi;
}

#endif
