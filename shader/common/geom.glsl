#ifndef GEOM_GLSL
#define GEOM_GLSL

#include "scene_buffers.glsl"

// hit_triangle_idx of a gbuffer-fed primary hit, which carries only the shading normal.
const uint NO_TRIANGLE = 0xFFFFFFFFu;

// Unit geometric (face) normal, in the hemisphere of vertex 0's shading normal. Not oriented
// toward any ray — offset_ray_origin() does that against the ray actually being spawned.
//
// Use this for self-intersection offsets rather than the shading normal, which near
// silhouettes can tilt past the tangent plane and push the origin into the surface.
vec3 triangle_geom_normal(uint tri_idx) {
    Triangle tri = triangles[tri_idx];
    vec3     gn  = normalize(cross(tri.e1, tri.e2));
    return dot(gn, vertices[tri.indices.x].normal) < 0.0 ? -gn : gn;
}

// Wächter & Binder, "A Fast and Robust Method for Avoiding Self-Intersection" (Ray Tracing
// Gems, ch. 6). Offsets by a fixed number of ULPs of the position, so the margin tracks the
// float error of a point at that magnitude at any scene scale; near the origin, where ULPs
// vanish, it falls back to a small absolute step.
const float RAY_OFFSET_ORIGIN      = 1.0 / 32.0;
const float RAY_OFFSET_FLOAT_SCALE = 1.0 / 65536.0;
const float RAY_OFFSET_INT_SCALE   = 256.0;

// Origin for a ray leaving the surface at `p` in direction `dir`, pushed off along `n` to
// whichever side `dir` leaves on — so reflection and refraction need no separate handling.
vec3 offset_ray_origin(vec3 p, vec3 n, vec3 dir) {
    n           = dot(n, dir) < 0.0 ? -n : n;
    ivec3 of_i  = ivec3(RAY_OFFSET_INT_SCALE * n);
    ivec3 bits  = floatBitsToInt(p) + ivec3(p.x < 0.0 ? -of_i.x : of_i.x, p.y < 0.0 ? -of_i.y : of_i.y, p.z < 0.0 ? -of_i.z : of_i.z);
    vec3  p_ulp = intBitsToFloat(bits);
    return mix(p_ulp, p + RAY_OFFSET_FLOAT_SCALE * n, lessThan(abs(p), vec3(RAY_OFFSET_ORIGIN)));
}

// A primary position reconstructed from depth carries error proportional to its view distance
// (measured at ~4e-6 per unit), not to its magnitude, so it gets an extra margin of this much
// per unit of camera distance on top of the ULP offset.
const float PRIMARY_OFFSET_PER_DEPTH = 2e-5;

vec3 offset_primary_origin(vec3 p, vec3 n, vec3 dir) {
    n = dot(n, dir) < 0.0 ? -n : n;
    return offset_ray_origin(p, n, dir) + n * (PRIMARY_OFFSET_PER_DEPTH * distance(p, camera_position));
}

// Ray origin for a path vertex: a traced hit offsets along its triangle's geometric normal,
// a gbuffer primary (no triangle) along its shading normal with the depth margin.
vec3 surface_ray_origin(vec3 p, uint tri_idx, vec3 shading_n, vec3 dir) {
    return (tri_idx != NO_TRIANGLE) ? offset_ray_origin(p, triangle_geom_normal(tri_idx), dir) : offset_primary_origin(p, shading_n, dir);
}

#endif
