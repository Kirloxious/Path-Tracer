#pragma once

#include <glm/glm.hpp>

#include "core/input.h"

struct CameraSettings
{
    float     aspect_ratio = 1.0f;
    int       image_width = 1200;
    int       samples_per_pixel = 4;
    int       max_bounces = 16;
    float     vfov = 90.0f;
    float     focus_dist = 10.0f;
    float     defocus_angle = 0.0f;
    glm::vec3 lookfrom = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 lookat = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 vup = glm::vec3(0.0f, 1.0f, 0.0f);
};

// POD sent directly to the GPU via a UBO (binding 2).
struct CameraData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::vec3 lookfrom; // camera position
    float     focus_distance;
    float     defocus_angle;
    float     _pad0[3];
    // proj * view from the previous frame, used for temporal reprojection
    // (ReSTIR temporal reuse, motion vectors). Equals current view_proj on the
    // very first frame — callers must guard temporal reuse with frame_index.
    glm::mat4 prev_view_proj;
};
static_assert(sizeof(CameraData) == 352, "CameraData must match std140 layout");

class Camera
{
public:
    CameraSettings settings;
    CameraData     data;

    int image_width;
    int image_height;

    glm::vec3 forward, right, up;
    float     yaw = 0.0f;
    float     pitch = 0.0f;

    bool  moving = false;
    float moveSpeed = 20.0f;
    float lookSpeed = 1.1f;

    explicit Camera(const CameraSettings& settings);

    // Applies one frame of input and returns *this for chaining.
    Camera& update(const InputState& input, float dt);

    // Update image dimensions + rebuild projection matrix to match a new aspect ratio.
    // Leaves position/orientation untouched.
    void resize(int width, int height);

    // Replace data.projection (and inv_projection) with a sub-pixel-jittered version,
    // using a Halton(2, 3) sequence indexed by frameIndex. AA falls out of progressive
    // accumulation: each frame the gbuffer samples a different sub-pixel position,
    // and accum_image's running average smooths the resulting edges.
    void applyJitter(int frameIndex);

private:
    void translate(glm::vec3 delta);
    void updateDirectionVectors();
    void updateViewMatrix();

    // Un-jittered projection, used as the base for applyJitter(). Set once at construction.
    glm::mat4 baseProjection = glm::mat4(1.0f);
};
