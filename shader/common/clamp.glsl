#ifndef CLAMP_GLSL
#define CLAMP_GLSL

#include "constants.glsl"

// Indirect contributions only, so authored emission is never rewritten. Scales the whole colour (clipping
// desaturates), capped on the max channel since luminance barely weights blue.
vec3 clamp_indirect(in vec3 contribution) {
    float m = max(contribution.r, max(contribution.g, contribution.b));
    return (m > indirect_clamp) ? contribution * (indirect_clamp / m) : contribution;
}

#endif
