#ifndef RESTIR_SURFACE_GLSL
#define RESTIR_SURFACE_GLSL

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

// Shadow-ray origin for the resampling vertex. Offset along the geometric normal, like
// path_continue: the shading normal can tilt past the tangent plane on smooth-shaded meshes
// and start the ray inside the surface. Primary vertices have no triangle and store the
// shading normal, matching the path tracer's fallback.
vec3 restir_surface_offset_origin(in RestirSurface s) {
    return s.position + 0.001 * restir_unpack_normal(s.offset_n);
}

#endif
