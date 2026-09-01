#ifndef BVH_TRAVERSAL_GLSL
#define BVH_TRAVERSAL_GLSL

#include "scene_buffers.glsl"
#include "rng.glsl"
#include "uniform_locations.glsl"

// Caller supplies the BVH root index as a uniform. Each pass that traces
// uses the same uniform name so the C++ side sets it once per pass.
layout(location = LOC_BVH_ROOT_INDEX) uniform int bvh_root_index;

// Index of the last emissive triangle. World::sortEmissiveFirst() stable-partitions
// emissives to the front, so `tri_index > emissive_last_index` is an exact test for
// "this triangle is not a light" — an integer compare against a uniform, replacing the
// dependent `mats[tri.material_index].type` fetch is_visible used to do for every
// candidate leaf. -1 when the scene has no emitters, making every triangle an occluder.
layout(location = LOC_EMISSIVE_LAST_INDEX) uniform int emissive_last_index;

// Leaf triangle references. A leaf owns the run [first, first + count); each entry indexes
// TrianglesBuffer. See BVH::triRefs for why the indirection exists.
layout(std430, binding = 26) readonly buffer TriRefsBuffer { int tri_refs[]; };

// Traversal stack depth. BVH::build logs the tree's actual max depth at load; this must
// stay above it, and 64 clears every scene here with room to spare. Overflow would silently
// drop a subtree, so pushes are guarded below rather than trusted.
const int BVH_STACK_SIZE = 64;

// Reciprocal ray direction for the slab test, with zero components nudged to a tiny but
// finite magnitude.
//
// A plain `1.0 / d` yields +-inf on an axis-aligned ray, and `aabb_min * inv_dir` then
// evaluates 0 * inf = NaN whenever that box face sits on the origin plane. GLSL leaves
// min()/max() unspecified for NaN inputs, so the node is accepted or rejected at the
// driver's whim. Cornell-box walls are axis-aligned and rays travelling along them are
// the common case here, not a corner case.
//
// 1e-8 keeps the reciprocal at 1e8: large enough that the axis stops constraining an
// interior ray, small enough that `aabb * 1e8` cannot overflow at any scene scale we
// build. A ray whose origin is outside the slab still gets two same-signed huge t values
// and is correctly rejected.
vec3 safe_inv_dir(in vec3 d) {
    const float eps = 1e-8;
    vec3        s   = vec3(d.x < 0.0 ? -eps : eps, d.y < 0.0 ? -eps : eps, d.z < 0.0 ? -eps : eps);
    return 1.0 / mix(d, s, lessThan(abs(d), vec3(eps)));
}

void set_face_normal_local(in vec3 ray_dir, in vec3 outward_normal, inout HitRecord hit) {
    hit.front_face = dot(ray_dir, outward_normal) < 0.0;
    hit.normal     = hit.front_face ? outward_normal : -outward_normal;
}

// Ray/triangle intersection reporting only the barycentric hit parameters. Vertex normals
// are deliberately NOT touched here: world_hit_stackless can accept several progressively
// closer triangles while descending, and interpolating + normalizing a shading normal for
// each one is work the final hit throws away. Deferring it past the loop also shrinks the
// live register set inside the hottest loop in the tracer, which buys occupancy.
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
    // `<=` on the slab compare so a ray exactly tangent to one face (t_near == t_far on
    // that axis) is still accepted — Cornell-box walls are axis-aligned and rays along
    // their plane otherwise miss the wall's node entirely.
    return t_near <= t_far && t_far > t_min && t_near < t_max;
}

// As above, but also reports where the ray enters the box. Ordered traversal descends into
// whichever child it enters first, so that the nearer subtree gets a chance to shrink
// `closest` before the farther one is ever tested.
bool intersect_aabb_entry(in vec3 mn, in vec3 mx, in vec3 inv_dir, in vec3 neg_ood, in float t_min, in float t_max, out float entry) {
    vec3  t1     = mn * inv_dir + neg_ood;
    vec3  t2     = mx * inv_dir + neg_ood;
    float t_near = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    float t_far  = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));
    entry        = t_near;
    return t_near <= t_far && t_far > t_min && t_near < t_max;
}

// Closest-hit traversal. Returns triangle index in `out_tri_index` for callers
// that need it (e.g. NEE light identification).
//
// Ordered: at each interior node both children are slab-tested, the nearer is descended
// into and the farther is pushed. That lets the near subtree shrink `closest` first, so the
// far subtree is often rejected outright — the skip-pointer scheme this replaced always
// walked left-first and had no way to express the ordering.
bool world_hit_stackless(in Ray r, in float t_min, in float t_max, out HitRecord hit, out int out_tri_index) {
    vec3  inv_dir  = safe_inv_dir(r.direction);
    vec3  neg_ood  = -r.origin * inv_dir;
    float closest  = t_max;
    int   best_tri = -1;
    vec2  best_uv  = vec2(0.0);

    int stack[BVH_STACK_SIZE];
    int sp  = 0;
    int idx = bvh_root_index;

    // Reject the whole tree up front so the loop below can assume the current node's box
    // was already tested by whoever scheduled it.
    {
        vec4 amin = nodes[idx].aabb_min;
        vec4 amax = nodes[idx].aabb_max;
        if (!intersect_aabb(amin.xyz, amax.xyz, inv_dir, neg_ood, t_min, closest)) {
            out_tri_index = -1;
            return false;
        }
    }

    while (true) {
        vec4 amin  = nodes[idx].aabb_min;
        vec4 amax  = nodes[idx].aabb_max;
        int  count = floatBitsToInt(amax.w);

        if (count > 0) {
            int first = floatBitsToInt(amin.w);
            for (int i = 0; i < count; ++i) {
                int   tri_idx = tri_refs[first + i];
                float t;
                vec2  uv;
                if (hit_triangle_uv(r, triangles[tri_idx], t_min, closest, t, uv)) {
                    closest  = t;
                    best_uv  = uv;
                    best_tri = tri_idx;
                }
            }
            if (sp == 0) break;
            idx = stack[--sp];
            continue;
        }

        int left  = idx + 1;
        int right = floatBitsToInt(amin.w);

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

    out_tri_index = best_tri;
    if (best_tri < 0) return false;

    // Hit attributes, computed exactly once for the surviving triangle.
    Triangle tri      = triangles[best_tri];
    vec3     n0       = vertices[tri.indices.x].normal;
    vec3     n1       = vertices[tri.indices.y].normal;
    vec3     n2       = vertices[tri.indices.z].normal;
    vec3     n_interp = normalize((1.0 - best_uv.x - best_uv.y) * n0 + best_uv.x * n1 + best_uv.y * n2);

    hit.t              = closest;
    hit.point          = r.origin + closest * r.direction;
    hit.mat_index      = tri.material_index;
    hit.triangle_index = uint(best_tri);
    set_face_normal_local(r.direction, n_interp, hit);
    return true;
}

// Traversal-cost probe for the BVH-cost AOV: walks exactly as world_hit_stackless does,
// including the ordering and the `closest` early-out, but accumulates a step count instead
// of hit attributes. Lives here rather than in aov.comp so there is one traversal loop in
// the tree — the duplicate copy silently stopped matching the moment the node layout
// changed, which is precisely the drift this avoids.
uint world_hit_cost(in Ray r, in float t_min, in float t_max) {
    vec3  inv_dir = safe_inv_dir(r.direction);
    vec3  neg_ood = -r.origin * inv_dir;
    float closest = t_max;
    uint  cost    = 0u;

    int stack[BVH_STACK_SIZE];
    int sp  = 0;
    int idx = bvh_root_index;

    {
        cost += 1u;
        vec4 amin = nodes[idx].aabb_min;
        vec4 amax = nodes[idx].aabb_max;
        if (!intersect_aabb(amin.xyz, amax.xyz, inv_dir, neg_ood, t_min, closest)) {
            return cost;
        }
    }

    while (true) {
        vec4 amin  = nodes[idx].aabb_min;
        vec4 amax  = nodes[idx].aabb_max;
        int  count = floatBitsToInt(amax.w);

        if (count > 0) {
            int first = floatBitsToInt(amin.w);
            for (int i = 0; i < count; ++i) {
                cost += 2u; // a triangle test costs more than a slab test — weight it
                float t;
                vec2  uv;
                if (hit_triangle_uv(r, triangles[tri_refs[first + i]], t_min, closest, t, uv)) {
                    closest = t;
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
    return cost;
}

// Shadow visibility — emissives are light sources, not occluders.
//
// Any-hit, so there is nothing to gain from near-first ordering: the first occluder found
// ends the walk whatever order they come in. Still stack-based, because the node layout no
// longer carries skip pointers.
bool is_visible(in vec3 origin, in vec3 target) {
    vec3  d        = target - origin;
    float dist2    = dot(d, d);
    float inv_dist = inversesqrt(dist2);
    vec3  ndir     = d * inv_dist;
    float dist     = dist2 * inv_dist;

    Ray   shadow  = Ray(origin, ndir);
    vec3  inv_dir = safe_inv_dir(ndir);
    vec3  neg_ood = -shadow.origin * inv_dir;
    float t_min   = 0.001;
    float t_max   = dist - 0.001;

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
                    // Emissive prefix test first: a pure integer compare, so a light leaf
                    // costs nothing beyond the node fetch already paid for.
                    if (tri_idx > emissive_last_index && hit_triangle_any(shadow, triangles[tri_idx], t_min, t_max)) {
                        return false;
                    }
                }
            } else {
                // Both children are candidates; descend left, defer right.
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
