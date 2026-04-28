#ifndef SCENE_BUFFERS_GLSL
#define SCENE_BUFFERS_GLSL

#include "primitives.glsl"

// Scene SSBOs/UBO mirroring Renderer's loadScene bindings. Every wavefront
// stage that touches the scene includes this header; the C++ side binds
// the underlying buffers exactly once at scene load.

layout(std430, binding = 0) readonly buffer LightGroupsBuffer { LightGroup light_groups[]; };
layout(std430, binding = 1) readonly buffer MatsBuffer        { Material   mats[]; };

layout(std140, binding = 2) uniform Camera {
    mat4  view_matrix;
    mat4  proj_matrix;
    mat4  inv_view_matrix;
    mat4  inv_proj_matrix;
    vec3  camera_position;
    float focus_distance;
    float defocus_angle;
};

layout(std430, binding = 3) readonly buffer BVHBuffer       { BVHNodeFlat nodes[]; };
layout(std430, binding = 4) readonly buffer TrianglesBuffer { Triangle    triangles[]; };
layout(std430, binding = 5) readonly buffer VerticesBuffer  { Vertex      vertices[]; };

#endif
