#version 460 core

#include "common/camera.glsl"

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in uint a_matid;

out vec3 v_world_pos;
out vec3 v_world_normal;
flat out uint v_matid;

void main() {
    v_world_pos = a_pos;
    v_world_normal = a_normal;
    v_matid = a_matid;

    // Parenthesised so it is two mat4 x vec4, not a mat4 x mat4 per vertex.
    gl_Position = proj_matrix * (view_matrix * vec4(a_pos, 1.0));
}
