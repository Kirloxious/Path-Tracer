#ifndef GBUFFER_GLSL
#define GBUFFER_GLSL

#include "scene_buffers.glsl"
#include "host_shared.glsl"

// Primary-visibility reads.
//
// The G-buffer stores a normal and a depth, and nothing else. World position used to live
// in its own rgba32f target — 16 bytes per pixel, read by six kernels — which duplicated
// information the depth attachment already held for depth testing. Reconstructing it costs
// one inverse-projection matmul and saves that whole target.
//
// This is only accurate because the projection is reversed-Z (see makeReversedZProjection
// in camera.cpp). With the conventional mapping the reconstruction was measured at 2.2e-2
// world units of error on Cornell Box, far past any usable self-intersection offset.
// Reversed-Z brings it to 2.9e-4 worst case; offset_primary_origin() sizes its margin on that.
layout(binding = TEX_GBUF_NORMAL) uniform sampler2D gbuf_normal_tex;
layout(binding = TEX_GBUF_DEPTH) uniform sampler2D gbuf_depth_tex;

// Raw normal, un-normalized. Length zero means nothing was rasterized at this pixel — the
// sky sentinel every consumer tests against.
vec3 gbuffer_raw_normal(in ivec2 px) {
    return texelFetch(gbuf_normal_tex, px, 0).xyz;
}

bool gbuffer_is_sky(in vec3 raw_normal) {
    return dot(raw_normal, raw_normal) < 0.5;
}

uint gbuffer_matid(in ivec2 px) {
    return uint(texelFetch(gbuf_normal_tex, px, 0).w);
}

// View-space position of the primary hit.
//
// glClipControl(GL_ZERO_TO_ONE) means the stored depth *is* the clip-space z, so it goes
// into the NDC vector verbatim with no [-1,1] remap. `inv_proj_matrix` is rebuilt from the
// jittered projection every frame, matching the matrix the rasterizer used, so this lands
// on the same sub-pixel sample the raster shaded.
//
// Exposed separately because the view transform is rigid: a point-to-plane distance is the
// same in either space, so the denoiser's plane edge-stop can stop at this one and skip the
// second matmul on all 25 of its taps.
vec3 gbuffer_view_pos(in ivec2 px, in ivec2 image_size) {
    vec2 uv  = (vec2(px) + 0.5) / vec2(image_size);
    vec3 ndc = vec3(uv * 2.0 - 1.0, texelFetch(gbuf_depth_tex, px, 0).r);
    vec4 v   = inv_proj_matrix * vec4(ndc, 1.0);
    return v.xyz / v.w; // undo the perspective divide
}

vec3 gbuffer_world_pos(in ivec2 px, in ivec2 image_size) {
    return (inv_view_matrix * vec4(gbuffer_view_pos(px, image_size), 1.0)).xyz;
}

#endif
