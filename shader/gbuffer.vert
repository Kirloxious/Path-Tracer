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

out vec3 v_world_pos;
out vec3 v_world_normal;
flat out uint v_matid;

void main() {
    v_world_pos = a_pos;
    v_world_normal = a_normal;
    v_matid = a_matid;

    gl_Position = camera.projection * camera.view * vec4(a_pos, 1.0);
}
