#ifndef CLAMP_GLSL
#define CLAMP_GLSL

#include "uniform_locations.glsl"

// Firefly ceiling for *indirect* radiance contributions.
//
// This used to live in resolve.comp as a blanket clamp on the whole accumulated
// radiance, which also capped directly-visible emitters: a Cornell light authored at
// emission = 8 and a SphereWorld sun at 10 were both silently rewritten to <= 10 no
// matter what the artist typed, and the clamp biased every path.
//
// Clamping at the point of contribution instead lets the primary hit on a light
// through untouched (shade_emissive at bounce 0, generate.comp's primary sky) while
// still bounding the estimator outputs that actually spike: NEE against a small solid
// angle, ReSTIR's W, and emissives found by a BSDF ray after several bounces.
layout(location = LOC_INDIRECT_CLAMP) uniform float indirect_clamp;

vec3 clamp_indirect(in vec3 contribution) {
    return min(contribution, vec3(indirect_clamp));
}

#endif
