#ifndef SCENE_BUFFERS_GLSL
#define SCENE_BUFFERS_GLSL

#include "primitives.glsl"
#include "constants.glsl"
#include "camera.glsl"

layout(std430, binding = BIND_LIGHT_GROUPS) readonly buffer LightGroupsBuffer { LightGroup light_groups[]; };
layout(std430, binding = BIND_MATERIALS) readonly buffer MatsBuffer        { Material   mats[]; };
layout(std430, binding = BIND_BVH_NODES) readonly buffer BVHBuffer       { BVHNodeFlat nodes[]; };
layout(std430, binding = BIND_TRIANGLES) readonly buffer TrianglesBuffer { Triangle    triangles[]; };
layout(std430, binding = BIND_VERTICES) readonly buffer VerticesBuffer  { Vertex      vertices[]; };

#endif
