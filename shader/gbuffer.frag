#version 460 core

in vec3 v_world_pos;
in vec3 v_world_normal;
flat in uint v_matid;

layout(std140, binding = 2) uniform CameraData
{
    mat4  view;
    mat4  projection;
    mat4  inv_view;
    mat4  inv_projection;
    vec3  lookfrom;
    float focus_distance;
    float defocus_angle;
} camera;

layout(location = 0) out vec4 o_pos_matid;
layout(location = 1) out vec4 o_normal;

void main() {
    // Match the BVH's set_face_normal convention: flip the stored outward normal so
    // it always faces the camera. Using gl_FrontFacing is fragile — it depends on
    // triangle winding, which addSphere does not align with the outward vertex normal,
    // so the floor-sphere top renders with an inverted normal under that scheme.
    vec3 N        = normalize(v_world_normal);
    vec3 view_dir = normalize(v_world_pos - camera.lookfrom);
    if (dot(view_dir, N) > 0.0) {
        N = -N;
    }

    o_pos_matid = vec4(v_world_pos, uintBitsToFloat(v_matid));
    o_normal    = vec4(N, 0.0);
}
