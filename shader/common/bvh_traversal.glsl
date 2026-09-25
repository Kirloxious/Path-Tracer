#ifndef BVH_TRAVERSAL_GLSL
#define BVH_TRAVERSAL_GLSL

#include "scene_buffers.glsl"
#include "rng.glsl"
// Leaf -> triangle indirection, so leaves batch triangles without disturbing the emissive-first order.
layout(std430, binding = BIND_TRI_REFS) readonly buffer TriRefsBuffer { int tri_refs[]; };

// Must exceed the depth BVH::build logs: an overflowing push is dropped, silently losing that subtree.
const int BVH_STACK_SIZE = 64;

// Zero components nudged to +-1e-8: 1/0 = inf gives 0 * inf = NaN where a box face sits on the origin
// plane, and min()/max() on NaN are unspecified.
vec3 safe_inv_dir(in vec3 d) {
    const float eps = 1e-8;
    vec3        s   = vec3(d.x < 0.0 ? -eps : eps, d.y < 0.0 ? -eps : eps, d.z < 0.0 ? -eps : eps);
    return 1.0 / mix(d, s, lessThan(abs(d), vec3(eps)));
}

void set_face_normal_local(in vec3 ray_dir, in vec3 outward_normal, inout HitRecord hit) {
    hit.front_face = dot(ray_dir, outward_normal) < 0.0;
    hit.normal     = hit.front_face ? outward_normal : -outward_normal;
}

// Reports only t and barycentrics; shading attributes are computed once for the final hit, keeping the
// traversal loop's register set small.
bool hit_triangle_uv(in Ray r, in Triangle tri, float t_min, float t_max, out float out_t, out vec2 out_uv) {
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

    out_t  = t;
    out_uv = vec2(u, v);
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
    // `<=` so a ray in the plane of an axis-aligned wall (t_near == t_far) still enters its node.
    return t_near <= t_far && t_far > t_min && t_near < t_max;
}

bool intersect_aabb_entry(in vec3 mn, in vec3 mx, in vec3 inv_dir, in vec3 neg_ood, in float t_min, in float t_max, out float entry) {
    vec3  t1     = mn * inv_dir + neg_ood;
    vec3  t2     = mx * inv_dir + neg_ood;
    float t_near = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    float t_far  = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));
    entry        = t_near;
    return t_near <= t_far && t_far > t_min && t_near < t_max;
}

// Pass as a `skip_tri` argument when there is no triangle to exclude.
const int NO_SKIP_TRI = -1;

// Ordered closest-hit traversal. Skipping `skip_tri` (the triangle the ray leaves) makes self-hits on it
// impossible, so the origin offset only has to clear neighbours. out_cost weights triangle tests double.
int bvh_closest_hit(in Ray r, float t_min, float t_max, int skip_tri, out float out_t, out vec2 out_uv, out uint out_cost) {
    vec3  inv_dir  = safe_inv_dir(r.direction);
    vec3  neg_ood  = -r.origin * inv_dir;
    float closest  = t_max;
    int   best_tri = -1;
    vec2  best_uv  = vec2(0.0);
    uint  cost     = 1u;

    out_t    = t_max;
    out_uv   = vec2(0.0);
    out_cost = cost;

    int stack[BVH_STACK_SIZE];
    int sp  = 0;
    int idx = bvh_root_index;

    // Tested up front so the loop can assume every visited node already passed its box test.
    if (!intersect_aabb(nodes[idx].aabb_min.xyz, nodes[idx].aabb_max.xyz, inv_dir, neg_ood, t_min, closest)) {
        return -1;
    }

    while (true) {
        vec4 amin  = nodes[idx].aabb_min;
        vec4 amax  = nodes[idx].aabb_max;
        int  count = floatBitsToInt(amax.w);

        if (count > 0) {
            // A popped leaf passed its box test against an older, larger `closest`; re-test to cull it.
            cost += 1u;
            if (intersect_aabb(amin.xyz, amax.xyz, inv_dir, neg_ood, t_min, closest)) {
                int first = floatBitsToInt(amin.w);
                for (int i = 0; i < count; ++i) {
                    int tri_idx = tri_refs[first + i];
                    if (tri_idx == skip_tri) continue;
                    cost += 2u;
                    float t;
                    vec2  uv;
                    if (hit_triangle_uv(r, triangles[tri_idx], t_min, closest, t, uv)) {
                        closest  = t;
                        best_uv  = uv;
                        best_tri = tri_idx;
                    }
                }
            }
            if (sp == 0) break;
            idx = stack[--sp];
            continue;
        }

        int left  = idx + 1;
        int right = floatBitsToInt(amin.w);

        cost += 2u;
        float entry_l;
        float entry_r;
        bool  hit_l = intersect_aabb_entry(nodes[left].aabb_min.xyz, nodes[left].aabb_max.xyz, inv_dir, neg_ood, t_min, closest, entry_l);
        bool  hit_r = intersect_aabb_entry(nodes[right].aabb_min.xyz, nodes[right].aabb_max.xyz, inv_dir, neg_ood, t_min, closest, entry_r);

        if (hit_l && hit_r) {
            int near_child = (entry_l <= entry_r) ? left : right;
            int far_child  = (entry_l <= entry_r) ? right : left;
            if (sp < BVH_STACK_SIZE) {
                stack[sp++] = far_child;
            }
            idx = near_child;
        } else if (hit_l) {
            idx = left;
        } else if (hit_r) {
            idx = right;
        } else {
            if (sp == 0) break;
            idx = stack[--sp];
        }
    }

    out_t    = closest;
    out_uv   = best_uv;
    out_cost = cost;
    return best_tri;
}

bool world_hit(in Ray r, in float t_min, in float t_max, in int skip_tri, out HitRecord hit, out int out_tri_index) {
    float t;
    vec2  uv;
    uint  cost;
    int   best_tri = bvh_closest_hit(r, t_min, t_max, skip_tri, t, uv, cost);
    out_tri_index  = best_tri;
    if (best_tri < 0) return false;

    Triangle tri      = triangles[best_tri];
    vec3     n0       = vertices[tri.indices.x].normal;
    vec3     n1       = vertices[tri.indices.y].normal;
    vec3     n2       = vertices[tri.indices.z].normal;
    vec3     n_interp = normalize((1.0 - uv.x - uv.y) * n0 + uv.x * n1 + uv.y * n2);

    // From barycentrics, not origin + t * dir, so the error scales with vertex magnitude (what
    // offset_ray_origin()'s ULP margin is sized for) rather than ray length.
    hit.t              = t;
    hit.point          = vertices[tri.indices.x].position + uv.x * tri.e1 + uv.y * tri.e2;
    hit.mat_index      = tri.material_index;
    hit.triangle_index = uint(best_tri);
    set_face_normal_local(r.direction, n_interp, hit);
    return true;
}

uint world_hit_cost(in Ray r, in float t_min, in float t_max) {
    float t;
    vec2  uv;
    uint  cost;
    bvh_closest_hit(r, t_min, t_max, NO_SKIP_TRI, t, uv, cost);
    return cost;
}

// Relative, so the target's neighbouring triangles are not hit at t ~ dist at any scene scale.
const float SHADOW_T_MAX_SCALE = 1.0 - 1e-4;

// Emitters occlude like any surface: a BSDF ray stops at them, and NEE must agree or the MIS pair
// integrates different things. origin_tri and target_tri are exempt.
bool is_visible(in vec3 origin, in vec3 target, int origin_tri, int target_tri) {
    vec3  d        = target - origin;
    float dist2    = dot(d, d);
    float inv_dist = inversesqrt(dist2);
    vec3  ndir     = d * inv_dist;
    float dist     = dist2 * inv_dist;

    Ray   shadow  = Ray(origin, ndir);
    vec3  inv_dir = safe_inv_dir(ndir);
    vec3  neg_ood = -shadow.origin * inv_dir;
    float t_min   = 0.0;
    float t_max   = dist * SHADOW_T_MAX_SCALE;

    int stack[BVH_STACK_SIZE];
    int sp  = 0;
    int idx = bvh_root_index;

    while (true) {
        vec4 amin  = nodes[idx].aabb_min;
        vec4 amax  = nodes[idx].aabb_max;
        int  count = floatBitsToInt(amax.w);

        if (intersect_aabb(amin.xyz, amax.xyz, inv_dir, neg_ood, t_min, t_max)) {
            if (count > 0) {
                int first = floatBitsToInt(amin.w);
                for (int i = 0; i < count; ++i) {
                    int tri_idx = tri_refs[first + i];
                    if (tri_idx != origin_tri && tri_idx != target_tri && hit_triangle_any(shadow, triangles[tri_idx], t_min, t_max)) {
                        return false;
                    }
                }
            } else {
                if (sp < BVH_STACK_SIZE) {
                    stack[sp++] = floatBitsToInt(amin.w);
                }
                idx = idx + 1;
                continue;
            }
        }

        if (sp == 0) break;
        idx = stack[--sp];
    }
    return true;
}

#endif
