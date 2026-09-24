#ifndef UNIFORM_LOCATIONS_GLSL
#define UNIFORM_LOCATIONS_GLSL

// Explicit `layout(location = N) uniform` numbers owned by a common/ header.
//
// Locations are a per-program namespace, so a kernel's private uniforms number from 0 and
// may reuse each other's slots freely. A uniform declared in a shared header must claim the
// same number in every program that includes it; those live here, from 8 up, so they cannot
// collide with any kernel's private range.
//
// Frame- and scene-wide values are not uniforms at all — see constants.glsl.

#define LOC_BOUNCE_INDEX 8 // common/path_continue.glsl

#endif
