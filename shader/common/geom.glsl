#ifndef GEOM_GLSL
#define GEOM_GLSL

#include "scene_buffers.glsl"

// Geometric (face) normal of a triangle, flipped to match the front-face
// convention from bvh_traversal's set_face_normal: front_face == true means
// the ray hit the side the normal already points toward; otherwise we flip.
//
// Use this for self-intersection offsets — `hit_normal` in PathState is the
// *shading* normal (smoothed across vertex normals). On curved meshes near
// silhouettes the shading normal tilts past the tangent plane and offsetting
// along it pushes the new ray origin *into* the surface, causing the next
// trace to self-hit.
//
// Winding caveat: cross(e1, e2) is *outward* only when the triangle is wound
// CCW relative to its outward direction. `addSphere` in world.h emits CW
// triangles relative to its outward-pointing vertex normals, so cross(e1, e2)
// points inward for sphere geometry. We canonicalize by flipping into the
// hemisphere of vertex[0]'s normal (which is always authored outward).
vec3 triangle_geom_normal(uint tri_idx, bool front_face) {
    Triangle tri = triangles[tri_idx];
    vec3 gn = normalize(cross(tri.e1, tri.e2));
    vec3 vn0 = vertices[tri.indices.x].normal;
    if (dot(gn, vn0) < 0.0) gn = -gn;
    return front_face ? gn : -gn;
}

#endif
