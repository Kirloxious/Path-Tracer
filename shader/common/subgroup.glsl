#ifndef SUBGROUP_GLSL
#define SUBGROUP_GLSL

// Must be included immediately after #version: #extension has to precede every declaration.
//
// ARB_shader_ballot rather than KHR_shader_subgroup_*, which NVIDIA's OpenGL driver does not
// advertise. It needs ARB_gpu_shader_int64 for its uint64_t ballot mask.
//
// `enable`, not `require`, so a driver without them still links; queue.glsl falls back to a
// per-thread atomic.
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_shader_ballot : enable

#endif
