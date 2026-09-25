#pragma once

#include <cstddef>
#include <glm/glm.hpp>

#include "core/input.h"

struct CameraSettings
{
    float aspect_ratio = 1.0f;
    int   image_width = 1200;
    int   max_bounces = 16;
    /// Caps indirect contributions only; directly visible emission is exempt so lights render at
    /// their authored brightness.
    float     indirect_clamp = 10.0f;
    float     vfov = 90.0f; ///< Vertical, degrees.
    float     focus_dist = 10.0f;
    float     defocus_angle = 0.0f; ///< Degrees; 0 disables depth of field.
    glm::vec3 lookfrom = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 lookat = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 vup = glm::vec3(0.0f, 1.0f, 0.0f);
};

/// Mirrors the std140 `Camera` block (binding 2).
struct CameraData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::vec3 lookfrom;
    float     focus_distance;
    float     defocus_angle;
    float     _pad0[3];
    /// Built from the un-jittered projection so motion vectors describe surface motion only.
    /// Equals the current view_proj on the first frame; temporal reuse must guard on frame_index.
    glm::mat4 prev_view_proj;
};
static_assert(sizeof(CameraData) == 352, "CameraData must match std140 layout");
static_assert(offsetof(CameraData, projection) == 64);
static_assert(offsetof(CameraData, inv_view) == 128);
static_assert(offsetof(CameraData, inv_projection) == 192);
static_assert(offsetof(CameraData, lookfrom) == 256);
static_assert(offsetof(CameraData, focus_distance) == 268);
static_assert(offsetof(CameraData, defocus_angle) == 272);
static_assert(offsetof(CameraData, prev_view_proj) == 288);

class Camera
{
public:
    CameraSettings settings;
    CameraData     data;

    int image_width;
    int image_height;

    glm::vec3 forward, right, up;
    float     yaw = 0.0f;   ///< Degrees.
    float     pitch = 0.0f; ///< Degrees, clamped to ±89.

    /// Set by update() when input changed the view; Application restarts accumulation on it.
    bool  moving = false;
    float moveSpeed = 20.0f; ///< World units per second.
    float lookSpeed = 1.1f;  ///< Degrees per 1/60 s.

    explicit Camera(const CameraSettings& settings);

    void update(const InputState& input, float dt);

    void resize(int width, int height);

    /// Halton(2, 3) jitter within the inner half of the pixel, so silhouettes wobble less (TAA covers
    /// the lost reach). Always derived from `baseProjection`, so offsets never compound.
    void applyJitter(int frameIndex);

private:
    /// @param delta In the camera's local basis (x = right, y = up, z = forward).
    void translate(glm::vec3 delta);
    void updateDirectionVectors();
    void updateViewMatrix();

    glm::mat4 baseProjection = glm::mat4(1.0f);
};
