#ifndef CAMERA_GLSL
#define CAMERA_GLSL

#include "host_shared.glsl"

// Mirrors CameraData in src/scene/camera.h (std140).
layout(std140, binding = UBO_CAMERA) uniform Camera {
    mat4  view_matrix;
    mat4  proj_matrix;
    mat4  inv_view_matrix;
    mat4  inv_proj_matrix;
    vec3  camera_position;
    float focus_distance;
    float defocus_angle;
    mat4  prev_view_proj_matrix;
};

#endif
