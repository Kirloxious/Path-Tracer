#ifndef BVH_TRAVERSAL_GLSL
#define BVH_TRAVERSAL_GLSL

#include "scene_buffers.glsl"
#include "rng.glsl"

// Caller supplies the BVH root index as a uniform. Each pass that traces
// uses the same uniform name so the C++ side sets it once per pass.
layout(location = 8) uniform int bvh_root_index;

void set_face_normal_local(in vec3 ray_dir, in vec3 outward_normal, inout HitRecord hit) {
    hit.front_face = dot(ray_dir, outward_normal) < 0.0;
    hit.normal     = hit.front_face ? outward_normal : -outward_normal;
}

bool hit_triangle(in Ray r, in Triangle tri, float t_min, float t_max, inout HitRecord hit) {
    vec3  h = cross(r.direction, tri.e2);
    float a = dot(tri.e1, h);
    if (abs(a) < 1e-8) return false;

    vec3  v0 = vertices[tri.indices.x].position;
    float f  = 1.0 / a;
    vec3  s  = r.origin - v0;
    float u  = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    vec3  q = cross(s, tri.e1);
    float v = f * dot(r.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;

    float t = f * dot(tri.e2, q);
    if (t < t_min || t > t_max) return false;

    hit.t     = t;
    hit.point = r.origin + t * r.direction;

    vec3 n0 = vertices[tri.indices.x].normal;
    vec3 n1 = vertices[tri.indices.y].normal;
    vec3 n2 = vertices[tri.indices.z].normal;
    vec3 n_interp = normalize((1.0 - u - v) * n0 + u * n1 + v * n2);
    hit.mat_index = tri.material_index;
    set_face_normal_local(r.direction, n_interp, hit);
    return true;
}

bool hit_triangle_any(in Ray r, in Triangle tri, float t_min, float t_max) {
    vec3  h = cross(r.direction, tri.e2);
    float a = dot(tri.e1, h);
    if (abs(a) < 1e-8) return false;

    vec3  v0 = vertices[tri.indices.x].position;
    float f  = 1.0 / a;
    vec3  s  = r.origin - v0;
    float u  = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    vec3  q = cross(s, tri.e1);
    float v = f * dot(r.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;

    float t = f * dot(tri.e2, q);
    return t >= t_min && t <= t_max;
}

bool intersect_aabb(in vec3 mn, in vec3 mx, in vec3 inv_dir, in vec3 neg_ood, in float t_min, in float t_max) {
    vec3  t1     = mn * inv_dir + neg_ood;
    vec3  t2     = mx * inv_dir + neg_ood;
    float t_near = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    float t_far  = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));
    return t_near < t_far && t_far > t_min && t_near < t_max;
}

// Closest-hit traversal. Returns triangle index in `out_tri_index` for callers
// that need it (e.g. NEE light identification).
bool world_hit_stackless(in Ray r, in float t_min, in float t_max, out HitRecord hit, out int out_tri_index) {
    vec3  inv_dir = 1.0 / r.direction;
    vec3  neg_ood = -r.origin * inv_dir;
    int   idx     = bvh_root_index;
    bool  any     = false;
    float closest = t_max;
    out_tri_index = -1;

    while (idx >= 0) {
        BVHNodeFlat node = nodes[idx];
        if (intersect_aabb(node.aabb_min.xyz, node.aabb_max.xyz, inv_dir, neg_ood, t_min, closest)) {
            if (node.meta.x == -1) {
                HitRecord tmp;
                if (hit_triangle(r, triangles[node.meta.z], t_min, closest, tmp)) {
                    tmp.triangle_index = uint(node.meta.z);
                    closest            = tmp.t;
                    hit                = tmp;
                    any                = true;
                    out_tri_index      = node.meta.z;
                }
                idx = node.meta.w;
            } else {
                idx = node.meta.x;
            }
        } else {
            idx = node.meta.w;
        }
    }
    return any;
}

// Shadow visibility — emissives are light sources, not occluders.
bool is_visible(in vec3 origin, in vec3 target) {
    vec3  d        = target - origin;
    float dist2    = dot(d, d);
    float inv_dist = inversesqrt(dist2);
    vec3  ndir     = d * inv_dist;
    float dist     = dist2 * inv_dist;

    Ray  shadow   = Ray(origin, ndir);
    vec3 inv_dir  = 1.0 / ndir;
    vec3 neg_ood  = -shadow.origin * inv_dir;
    int  idx      = bvh_root_index;
    float t_min   = 0.001;
    float t_max   = dist - 0.001;

    while (idx >= 0) {
        BVHNodeFlat node = nodes[idx];
        if (intersect_aabb(node.aabb_min.xyz, node.aabb_max.xyz, inv_dir, neg_ood, t_min, t_max)) {
            if (node.meta.x == -1) {
                Triangle tri = triangles[node.meta.z];
                if (mats[tri.material_index].type != MAT_EMISSIVE) {
                    if (hit_triangle_any(shadow, tri, t_min, t_max)) return false;
                }
                idx = node.meta.w;
            } else {
                idx = node.meta.x;
            }
        } else {
            idx = node.meta.w;
        }
    }
    return true;
}

#endif
