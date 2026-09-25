#ifndef SUBGROUP_GLSL
#define SUBGROUP_GLSL

// Must follow #version directly. ARB_shader_ballot because NVIDIA's GL driver lacks KHR_shader_subgroup;
// `enable`, not `require`, so queue.glsl can fall back to per-thread atomics.
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_shader_ballot : enable

#endif
