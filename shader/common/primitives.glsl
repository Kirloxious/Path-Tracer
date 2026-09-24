#ifndef PRIMITIVES_GLSL
#define PRIMITIVES_GLSL

#include "host_shared.glsl"

//=============================================================================
// Structs
//=============================================================================

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
    uvec3 indices; // into vertices[]
    uint material_index;
    vec3 e1; // v1 - v0
    float area; // 0.5 * |e1 x e2|, precomputed on CPU
    vec3 e2; // v2 - v0
    uint alias_packed;  // emissive: alias-table entry — bits 31..16 unorm16 accept probability, bits 15..0 alias offset from group.begin
};

struct LightGroup
{
    int   begin;
    int   count;
    float total_area;
    uint  alias_packed;  // alias entry over *groups* — bits 31..16 unorm16 accept probability, 15..0 alias target
    float select_pdf;    // probability this group is chosen; power-weighted, so not 1/count
    float _pad0;
    float _pad1;
    float _pad2;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
};

// Mirrors src/scene/material.h. `roughness` is perceptual — GGX alpha is roughness^2.
// `type` is a derived MaterialClass cached on the CPU by Material::classify(), never authored,
// so every routing test below stays an exact integer compare.
struct Material
{
    vec3 base_color;    // albedo (dielectric) or F0 tint (conductor)
    float metallic;     // 0 = dielectric, 1 = conductor
    vec3 emission;
    float roughness;    // perceptual; alpha = roughness^2, 0 = perfect mirror
    float ior;          // dielectric F0 = ((ior-1)/(ior+1))^2
    float transmission; // 0 = opaque, 1 = fully refractive
    uint type;          // MAT_* — cached MaterialClass
    float _pad0;
};

// Emitted radiance. `base_color` tints `emission`, so every estimator that touches a light —
// NEE, ReSTIR's target pdf, the BSDF-hits-emissive path, and the CPU's group power weighting —
// has to go through this or a tinted emitter casts light of a different colour than it shows.
vec3 material_emission(in Material mat) {
    return mat.base_color * mat.emission;
}

// 32 bytes: two vec4s, with the link data bit-cast into the .w lanes the box does not use.
// Two nodes per 64-byte cache line instead of 1.33 — traversal is bound on node fetches.
//
// The left child is implicit. BVH::flatten emits [self, left subtree, right subtree] depth
// first, so a node's left child is always the next slot and only the right one is stored.
//
//   interior: aabb_min.w = right child index,  aabb_max.w = 0
//   leaf:     aabb_min.w = first triangle ref, aabb_max.w = triangle count (> 0)
//
// Read the .w lanes back with floatBitsToInt. A count of 0 is what marks a node interior,
// so every leaf carries at least one triangle.
struct BVHNodeFlat
{
    vec4 aabb_min;
    vec4 aabb_max;
};

#endif
