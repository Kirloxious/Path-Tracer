#ifndef RESTIR_SURFACE_GLSL
#define RESTIR_SURFACE_GLSL

#include "scene_buffers.glsl"
#include "geom.glsl"

// The surface a pixel's reservoir actually describes.
//
// ReSTIR DI resamples direct lighting at a diffuse surface. For a diffuse primary that is
// simply the G-buffer hit, and the G-buffer was the only surface description the ReSTIR
// kernels needed. Once resampling follows a mirror chain, the reservoir's surface is no
// longer the surface the pixel rasterized — the mirror floor's reservoir describes the
// sphere reflected in it, several metres away and facing a different direction.
//
// So the resampling surface is stored explicitly. Every place that used to read the
// G-buffer to get (P, N, albedo) for a reservoir reads this instead; the G-buffer is still
// what drives *reprojection*, because that is about which pixel sees the reflection.
//
// Stores the view direction rather than a cached albedo: the target pdf evaluates the full
// metallic-roughness BRDF, which is view-dependent and needs every material parameter, so it
// refetches the Material through `matid` instead. That keeps the struct at 48 bytes — it is
// read k=5 times per spatial pass, twice per frame, so growing it is not free.
//
// 48 bytes, std430.
struct RestirSurface {
    vec3 position;  // world-space resampling vertex
    uint valid;     // 0 = this pixel has no diffuse resampling vertex (sky, emissive, fuzzy specular)
    vec3 normal;    // shading normal there, already normalized and front-facing
    uint matid;     // material at the resampling vertex, for reuse validation
    vec3 view_dir;  // unit vector from the resampling vertex toward the viewer, for the BRDF
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

// Shadow-ray origin toward `target`. `offset_n` is the geometric normal past a mirror and the
// shading normal at a primary. Both take the primary's depth margin, since the struct does not
// record which it is — on a shadow ray a slightly larger offset past a mirror costs nothing.
vec3 restir_surface_offset_origin(in RestirSurface s, vec3 target) {
    return offset_primary_origin(s.position, restir_unpack_normal(s.offset_n), target - s.position);
}

// Reuse gate between two resampling surfaces (temporal history, spatial neighbour).
//
// The plane distance, not the Euclidean one: neighbours across a flat wall are valid reuse
// partners at any lateral distance, while a parallel surface behind a silhouette is not.
// Tolerance is relative to view distance so it means the same at any scene scale.
const float RESTIR_NORMAL_DOT_MIN  = 0.9;
const float RESTIR_PLANE_DIST_REL  = 0.01;

bool restir_surfaces_similar(in RestirSurface self_s, in RestirSurface other) {
    if (other.valid == 0u || other.matid != self_s.matid) return false;
    if (dot(other.normal, self_s.normal) < RESTIR_NORMAL_DOT_MIN) return false;
    float plane_dist = abs(dot(self_s.normal, other.position - self_s.position));
    return plane_dist <= RESTIR_PLANE_DIST_REL * distance(camera_position, self_s.position);
}

#endif
