#ifndef UNIFORM_LOCATIONS_GLSL
#define UNIFORM_LOCATIONS_GLSL

// Uniform locations claimed by common/ headers. Kernels number their private uniforms from 0;
// these start at 8 so the two ranges cannot collide.

#define LOC_BOUNCE_INDEX 8 // common/path_continue.glsl

#endif
