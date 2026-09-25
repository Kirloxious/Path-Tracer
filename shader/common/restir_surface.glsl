#ifndef RESTIR_SURFACE_GLSL
#define RESTIR_SURFACE_GLSL

#include "scene_buffers.glsl"
#include "geom.glsl"

// Past a mirror chain the reservoir's surface isn't the rasterized one, so reuse validation reads this
// while reprojection still uses the G-buffer. Material is refetched via `matid` to keep this 48 B.
struct RestirSurface {
    vec3 position;
    uint valid; // 0: sky, or the chain ended where restir_can_anchor() fails
    vec3 normal; // normalized, front-facing
    uint matid;
    vec3 view_dir; // toward the previous path vertex
    uint offset_n; // octahedral-packed
};

vec2 restir_oct_wrap(vec2 v) {
    return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

uint restir_pack_normal(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = (n.z >= 0.0) ? n.xy : restir_oct_wrap(n.xy);
    return packSnorm2x16(e);
}

vec3 restir_unpack_normal(uint p) {
    vec2 e = unpackSnorm2x16(p);
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = restir_oct_wrap(n.xy);
    }
    return normalize(n);
}

// The struct doesn't record whether `offset_n` is a shading or geometric normal, so both take the
// primary's depth margin.
vec3 restir_surface_offset_origin(in RestirSurface s, vec3 target) {
    return offset_primary_origin(s.position, restir_unpack_normal(s.offset_n), target - s.position);
}

// Plane distance, not Euclidean, so neighbours across a flat wall stay valid at any lateral distance;
// relative to view distance for scale independence.
const float RESTIR_NORMAL_DOT_MIN  = 0.9;
const float RESTIR_PLANE_DIST_REL  = 0.01;

bool restir_surfaces_similar(in RestirSurface self_s, in RestirSurface other) {
    if (other.valid == 0u || other.matid != self_s.matid) return false;
    if (dot(other.normal, self_s.normal) < RESTIR_NORMAL_DOT_MIN) return false;
    float plane_dist = abs(dot(self_s.normal, other.position - self_s.position));
    return plane_dist <= RESTIR_PLANE_DIST_REL * distance(camera_position, self_s.position);
}

#endif
