#pragma once

/**
 * @file camera.h
 * @brief Free-fly camera, its std140 GPU mirror, and the per-scene camera settings.
 */

#include <glm/glm.hpp>

#include "core/input.h"

/**
 * @brief Static per-scene camera configuration, supplied by each Scene factory.
 *
 * These are the values a scene author picks; Camera derives its matrices from them at
 * construction and thereafter mutates only position and orientation.
 */
struct CameraSettings
{
    float     aspect_ratio = 1.0f;
    int       image_width = 1200;
    int       max_bounces = 16;     ///< Path-tracer bounce budget per sample.
    float     vfov = 90.0f;         ///< Vertical field of view in degrees.
    float     focus_dist = 10.0f;   ///< Distance to the plane in perfect focus.
    float     defocus_angle = 0.0f; ///< Aperture angle in degrees; 0 disables depth of field.
    glm::vec3 lookfrom = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 lookat = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 vup = glm::vec3(0.0f, 1.0f, 0.0f);
};

/**
 * @brief POD sent directly to the GPU via the Camera UBO (binding 2).
 *
 * Layout must match the std140 `Camera` block in `scene_buffers.glsl` exactly; the
 * static_assert below pins the size.
 */
struct CameraData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::vec3 lookfrom; ///< Camera world position.
    float     focus_distance;
    float     defocus_angle;
    float     _pad0[3];
    /// `proj * view` from the previous frame, built from the *un-jittered* projection so
    /// motion vectors describe surface motion only. Used for temporal reprojection (ReSTIR
    /// temporal reuse, TAA). Equals the current view_proj on the very first frame — callers
    /// must guard temporal reuse with frame_index.
    glm::mat4 prev_view_proj;
};
static_assert(sizeof(CameraData) == 352, "CameraData must match std140 layout");

/**
 * @brief Free-fly camera with jittered projection for progressive anti-aliasing.
 *
 * Deliberately free of any GLFW dependency — it consumes the abstract InputState instead.
 * `data` is the exact block uploaded to the UBO each frame by Renderer::updateCameraUbo().
 */
class Camera
{
public:
    CameraSettings settings;
    CameraData     data;

    int image_width;
    int image_height;

    glm::vec3 forward, right, up;
    float     yaw = 0.0f;   ///< Radians about the world up axis.
    float     pitch = 0.0f; ///< Radians about the local right axis, clamped away from the poles.

    /// Set by update() when this frame's input changed the view. Application resets
    /// `frameIndex` on it, restarting progressive accumulation.
    bool  moving = false;
    float moveSpeed = 20.0f; ///< World units per second.
    float lookSpeed = 1.1f;  ///< Radians per second.

    /**
     * @brief Builds the initial basis, view and projection matrices from @p settings.
     * @param settings Per-scene camera configuration; copied.
     */
    explicit Camera(const CameraSettings& settings);

    /**
     * @brief Applies one frame of input, setting `moving` when anything changed.
     * @param input Action flags for this frame, from Window::pollInput().
     * @param dt    Seconds elapsed since the previous frame; scales both move and look speed.
     */
    void update(const InputState& input, float dt);

    /**
     * @brief Updates the image dimensions and rebuilds the projection for the new aspect ratio.
     *
     * Leaves position and orientation untouched.
     *
     * @param width  New framebuffer width in pixels.
     * @param height New framebuffer height in pixels.
     */
    void resize(int width, int height);

    /**
     * @brief Replaces `data.projection` (and its inverse) with a sub-pixel-jittered version.
     *
     * Uses a Halton(2, 3) sequence indexed by @p frameIndex, scaled to the inner half of the
     * pixel so silhouettes wobble less between frames (TAA covers the lost AA reach).
     * Anti-aliasing falls out of progressive accumulation: each frame the G-buffer samples a
     * different sub-pixel position, and the running average in `accum_image` smooths the
     * resulting edges. Always derived from `baseProjection`, so offsets never compound.
     *
     * @param frameIndex Current accumulation frame index; also seeds the Halton sample.
     */
    void applyJitter(int frameIndex);

private:
    /// @param delta Movement in the camera's local basis (x = right, y = up, z = forward).
    void translate(glm::vec3 delta);
    /// Rebuilds `forward` / `right` / `up` from the current yaw and pitch.
    void updateDirectionVectors();
    /// Rebuilds `data.view` and `data.inv_view` from the position and basis vectors.
    void updateViewMatrix();

    /// Un-jittered projection, used as the base for applyJitter(). Set once at construction
    /// and rebuilt by resize().
    glm::mat4 baseProjection = glm::mat4(1.0f);
};
