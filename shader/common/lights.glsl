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
int sample_light_triangle(in LightGroup grp, inout uint rng) {
    int slot = int(random_unilateral(rng) * float(grp.count));
    slot = min(slot, grp.count - 1); // guards random_unilateral() == 1.0 exactly

    // `packed` is a reserved word in GLSL, hence the name.
    uint  entry  = triangles[grp.begin + slot].alias_packed;
    float accept = float(entry >> 16) * (1.0 / 65535.0);
    if (random_unilateral(rng) >= accept) {
        slot = int(entry & 0xFFFFu);
    }
    return grp.begin + slot;
}

// Uniform pick over light groups, matching the `/ num_light_groups` factor every caller
// folds into its pdf.
int sample_light_group(in int num_light_groups, inout uint rng) {
    int gi = int(random_unilateral(rng) * float(num_light_groups));
    return min(gi, num_light_groups - 1);
}

#endif
