#ifndef GBUFFER_GLSL
#define GBUFFER_GLSL

#include "scene_buffers.glsl"
#include "host_shared.glsl"

// Primary-visibility reads. The G-buffer holds only a normal and a depth; position is
// reconstructed from depth, which is accurate only because the projection is reversed-Z
// (makeReversedZProjection in camera.cpp): 2.9e-4 worst case on Cornell Box against 2.2e-2
// conventionally. offset_primary_origin() sizes its margin on that.
layout(binding = TEX_GBUF_NORMAL) uniform sampler2D gbuf_normal_tex;
layout(binding = TEX_GBUF_DEPTH) uniform sampler2D gbuf_depth_tex;

// Un-normalized; zero length is the sky sentinel.
vec3 gbuffer_raw_normal(in ivec2 px) {
    return texelFetch(gbuf_normal_tex, px, 0).xyz;
}

bool gbuffer_is_sky(in vec3 raw_normal) {
    return dot(raw_normal, raw_normal) < 0.5;
}

uint gbuffer_matid(in ivec2 px) {
    return uint(texelFetch(gbuf_normal_tex, px, 0).w);
}

// View-space position of the primary hit. With GL_ZERO_TO_ONE clip control the stored depth is
// NDC z as is. `inv_proj_matrix` is the jittered projection the raster used, so this lands on
// the sub-pixel sample it shaded. Exposed separately for the denoiser, whose plane distances
// are the same in view space and skip the second matmul.
vec3 gbuffer_view_pos(in ivec2 px, in ivec2 image_size) {
    vec2 uv  = (vec2(px) + 0.5) / vec2(image_size);
    vec3 ndc = vec3(uv * 2.0 - 1.0, texelFetch(gbuf_depth_tex, px, 0).r);
    vec4 v   = inv_proj_matrix * vec4(ndc, 1.0);
    return v.xyz / v.w;
}

vec3 gbuffer_world_pos(in ivec2 px, in ivec2 image_size) {
    return (inv_view_matrix * vec4(gbuffer_view_pos(px, image_size), 1.0)).xyz;
}

#endif
