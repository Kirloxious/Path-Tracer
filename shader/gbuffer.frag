#version 460 core

#include "common/camera.glsl"

in vec3 v_world_pos;
in vec3 v_world_normal;
flat in uint v_matid;

layout(location = 0) out vec4 o_normal;

void main() {
    // Flipped toward the camera by the interpolated normal, matching the tracer's
    // set_face_normal; gl_FrontFacing goes by winding and would disagree where the two do.
    vec3 N        = normalize(v_world_normal);
    vec3 view_dir = normalize(v_world_pos - camera_position);
    if (dot(view_dir, N) > 0.0) {
        N = -N;
    }

    // Material index as a half float: exact below 2048.
    o_normal = vec4(N, float(v_matid));
}
