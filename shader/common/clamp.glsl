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

// Scales the whole contribution down rather than clipping each channel at the ceiling: a
// per-channel min() on a saturated spike removes only the channels that are over, which
// desaturates a firefly instead of dimming it.
//
// The ceiling is on the largest channel, not on luminance. Luminance weights green at 0.72 and
// blue at 0.07, so a ceiling on it lets a saturated spike through at several times the
// per-channel limit — the exact spikes a Cornell box produces, where every indirect bounce is
// strongly tinted. Bounding the max channel keeps `indirect_clamp` meaning what it always
// meant while still preserving hue.
vec3 clamp_indirect(in vec3 contribution) {
    float m = max(contribution.r, max(contribution.g, contribution.b));
    return (m > indirect_clamp) ? contribution * (indirect_clamp / m) : contribution;
}

#endif
