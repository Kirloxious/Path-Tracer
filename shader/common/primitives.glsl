#ifndef PRIMITIVES_GLSL
#define PRIMITIVES_GLSL

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
    int begin;
    int count;
    float total_area;
    float _pad;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
};

struct Material
{
    vec3 color;
    float fuzz;
    vec3 emission;
    float refractive_index;
    uint type;
};

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
