#ifndef PRIMITIVES_GLSL
#define PRIMITIVES_GLSL

#include "host_shared.glsl"

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct HitRecord
{
    vec3 point;
    vec3 normal;
    uint mat_index;
    uint triangle_index;
    float t;
    bool front_face;
};

struct Triangle
{
    uvec3 indices;
    uint material_index;
    vec3 e1;
    float area;
    vec3 e2;
    uint alias_packed; // emissive: bits 31..16 unorm16 accept, 15..0 alias offset from group.begin
};

struct LightGroup
{
    int   begin;
    int   count;
    float total_area;
    uint  alias_packed; // over groups: bits 31..16 unorm16 accept, 15..0 alias target
    float select_pdf; // power-weighted, so not 1/count
    float _pad0;
    float _pad1;
    float _pad2;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
};

// Mirrors material.h. `type` is derived on the CPU by Material::classify().
struct Material
{
    vec3 base_color;
    float metallic;
    vec3 emission;
    float roughness; // perceptual; alpha = roughness^2
    float ior;
    float transmission;
    uint type;
    float _pad0;
};

// Every light estimator (and the CPU's power weighting) must use this, or a tinted emitter casts a
// different colour than it shows.
vec3 material_emission(in Material mat) {
    return mat.base_color * mat.emission;
}

// Link data bit-cast into .w; left child is the next slot. Interior: min.w = right child, max.w = 0.
// Leaf: min.w = first triangle ref, max.w = count (> 0).
struct BVHNodeFlat
{
    vec4 aabb_min;
    vec4 aabb_max;
};

#endif
