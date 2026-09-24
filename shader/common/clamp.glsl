#ifndef CLAMP_GLSL
#define CLAMP_GLSL

#include "constants.glsl"

// Firefly ceiling for *indirect* contributions only, applied where each estimator adds its
// output. Directly visible emitters and sky are left untouched, so an authored emission is
// never rewritten.
//
// Scales the whole colour rather than clipping per channel, which would desaturate a firefly
// instead of dimming it. The ceiling is on the largest channel, not luminance: luminance barely
// weights blue, so a strongly tinted spike would pass at several times the limit.
vec3 clamp_indirect(in vec3 contribution) {
    float m = max(contribution.r, max(contribution.g, contribution.b));
    return (m > indirect_clamp) ? contribution * (indirect_clamp / m) : contribution;
}

#endif
