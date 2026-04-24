#version 460 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in uint a_matid;

layout(std140, binding = 2) uniform CameraData
{
    mat4 view;
    mat4 projection;
    mat4 inv_view;
    mat4 inv_projection;
    vec3 lookfrom;
    float focus_distance;
    float defocus_angle;
} camera;

// Previous-frame view-projection for screen-space motion vectors.
// Will be identity on the first frame; motion vectors are unused in phase 1.
uniform mat4 u_prev_view_proj;

out vec3 v_world_pos;
out vec3 v_world_normal;
flat out uint v_matid;
out vec4 v_curr_clip;
out vec4 v_prev_clip;

void main() {
    v_world_pos = a_pos;
    v_world_normal = a_normal;
    v_matid = a_matid;

    vec4 clip = camera.projection * camera.view * vec4(a_pos, 1.0);
    v_curr_clip = clip;
    v_prev_clip = u_prev_view_proj * vec4(a_pos, 1.0);
    gl_Position = clip;
}
