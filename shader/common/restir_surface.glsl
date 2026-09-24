#ifndef RESTIR_SURFACE_GLSL
#define RESTIR_SURFACE_GLSL

#include "scene_buffers.glsl"
#include "geom.glsl"

// The surface a pixel's reservoir describes. Past a mirror chain that is not the rasterized
// surface — a mirror floor's reservoir describes the sphere reflected in it — so reuse
// validation reads this, while reprojection still uses the G-buffer (which pixel saw it).
//
// 48 bytes, std430; read k times per spatial pass, so the Material is refetched through
// `matid` rather than cached here.
struct RestirSurface {
    vec3 position;  // world-space resampling vertex
    uint valid;     // 0 = no resampling vertex: sky, or a chain ending where restir_can_anchor() fails
    vec3 normal;    // shading normal there, already normalized and front-facing
    uint matid;     // material at the resampling vertex, for reuse validation
    vec3 view_dir;  // unit, toward the previous path vertex
    uint offset_n;  // octahedral-packed ray-origin offset normal; see restir_surface_offset_origin
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

// Shadow-ray origin toward `target`. The struct does not record whether `offset_n` is a primary's
// shading normal or a traced hit's geometric one, so both take the primary's depth margin.
vec3 restir_surface_offset_origin(in RestirSurface s, vec3 target) {
    return offset_primary_origin(s.position, restir_unpack_normal(s.offset_n), target - s.position);
}

// Reuse gate between two resampling surfaces. Plane distance, not Euclidean: neighbours across a
// flat wall are valid partners at any lateral distance. Relative to view distance, so it holds
// at any scene scale.
const float RESTIR_NORMAL_DOT_MIN  = 0.9;
const float RESTIR_PLANE_DIST_REL  = 0.01;

bool restir_surfaces_similar(in RestirSurface self_s, in RestirSurface other) {
    if (other.valid == 0u || other.matid != self_s.matid) return false;
    if (dot(other.normal, self_s.normal) < RESTIR_NORMAL_DOT_MIN) return false;
    float plane_dist = abs(dot(self_s.normal, other.position - self_s.position));
    return plane_dist <= RESTIR_PLANE_DIST_REL * distance(camera_position, self_s.position);
}

#endif
