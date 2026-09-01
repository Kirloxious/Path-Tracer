#ifndef SUBGROUP_GLSL
#define SUBGROUP_GLSL

// Must be included immediately after #version, before any declarations: GLSL requires
// #extension to precede non-preprocessor tokens. The `#line` directives the include
// preprocessor emits are themselves preprocessor directives, so they do not count.
//
// GL_ARB_shader_ballot, not the GL_KHR_shader_subgroup_* family — those are Vulkan-side and
// NVIDIA's OpenGL driver does not advertise them, so requiring them silently selected the
// per-thread fallback. ARB_shader_ballot is the OpenGL spelling and is present here.
// It needs ARB_gpu_shader_int64 for its uint64_t ballot mask.
//
// `enable` rather than `require` so a driver without them still links; queue.glsl falls
// back to a plain per-thread atomic under #ifdef.
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_shader_ballot : enable

#endif
