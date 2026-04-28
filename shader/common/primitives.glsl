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
    float cdf_in_group; // emissive: cumulative-area CDF within group; 1.0 at last; 0.0 elsewhere
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
    float type;
};

struct BVHNodeFlat
{
    vec4 aabb_min;
    vec4 aabb_max;
    ivec4 meta; // interior: x=left, y=right, z=-1,           w=skip
    // leaf:     x=-1,   y=0,     z=triangleIdx,  w=skip
};

#endif
