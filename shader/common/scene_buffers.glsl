#ifndef SCENE_BUFFERS_GLSL
#define SCENE_BUFFERS_GLSL

#include "primitives.glsl"
#include "constants.glsl"

// Scene SSBOs/UBO mirroring Renderer's loadScene bindings. Every wavefront
// stage that touches the scene includes this header; the C++ side binds
// the underlying buffers exactly once at scene load.

layout(std430, binding = BIND_LIGHT_GROUPS) readonly buffer LightGroupsBuffer { LightGroup light_groups[]; };
layout(std430, binding = BIND_MATERIALS) readonly buffer MatsBuffer        { Material   mats[]; };

layout(std140, binding = UBO_CAMERA) uniform Camera {
    mat4  view_matrix;
    mat4  proj_matrix;
    mat4  inv_view_matrix;
    mat4  inv_proj_matrix;
    vec3  camera_position;
    float focus_distance;
    float defocus_angle;
    mat4  prev_view_proj_matrix;
};

layout(std430, binding = BIND_BVH_NODES) readonly buffer BVHBuffer       { BVHNodeFlat nodes[]; };
layout(std430, binding = BIND_TRIANGLES) readonly buffer TrianglesBuffer { Triangle    triangles[]; };
layout(std430, binding = BIND_VERTICES) readonly buffer VerticesBuffer  { Vertex      vertices[]; };

#endif
