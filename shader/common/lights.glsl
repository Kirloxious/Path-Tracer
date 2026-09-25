#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#include "scene_buffers.glsl"
#include "rng.glsl"

// One-sided emitters. NEE, BSDF-hits-emissive and ReSTIR's target pdf must all apply this one test, or
// the MIS weights stop summing to one.
bool light_faces(in Triangle tri, vec3 light_dir) {
    return dot(cross(tri.e1, tri.e2), light_dir) < 0.0;
}

vec3 light_point(in Triangle tri, vec2 bary) {
    return vertices[tri.indices.x].position + bary.x * tri.e1 + bary.y * tri.e2;
}

vec2 sample_triangle_bary(inout Sampler smp) {
    vec2  u = sampler_2d(smp);
    float r = sqrt(u.x);
    return vec2(r * (1.0 - u.y), r * u.y);
}

// Group selection pdf times uniform density over the group's whole area, so a tessellated emitter acts
// as one light. Every estimator MIS-weighted against NEE must use this density.
float light_solid_angle_pdf(in Triangle tri, in LightGroup grp, vec3 light_dir, float dist2) {
    if (!light_faces(tri, light_dir) || grp.total_area <= 0.0) {
        return 0.0;
    }
    // -dot(cross(e1, e2), L) is 2 * area * cos at the light.
    return grp.select_pdf * -2.0 * tri.area * dist2 / (dot(cross(tri.e1, tri.e2), light_dir) * grp.total_area);
}

// Alias pick with probability exactly area / total_area, as light_solid_angle_pdf() assumes. Returns an
// absolute triangle index.
int sample_light_triangle(in LightGroup grp, inout Sampler smp) {
    // One 2D draw: the alias method needs each coordinate uniform, not independent.
    vec2 u    = sampler_2d(smp);
    int  slot = min(int(u.x * float(grp.count)), grp.count - 1);

    // `packed` is a reserved word in GLSL.
    uint  entry  = triangles[grp.begin + slot].alias_packed;
    float accept = float(entry >> 16) * (1.0 / 65535.0);
    if (u.y >= accept) {
        slot = int(entry & 0xFFFFu);
    }
    return grp.begin + slot;
}

int sample_light_group(in int num_light_groups, inout Sampler smp) {
    vec2 u  = sampler_2d(smp);
    int  gi = min(int(u.x * float(num_light_groups)), num_light_groups - 1);

    uint  entry  = light_groups[gi].alias_packed;
    float accept = float(entry >> 16) * (1.0 / 65535.0);
    return (u.y >= accept) ? int(entry & 0xFFFFu) : gi;
}

#endif
