#include "scene/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include "core/log.h"

Camera::Camera(const CameraSettings& settings)
    : settings(settings), image_width(settings.image_width), image_height(static_cast<int>(settings.image_width / settings.aspect_ratio)) {

    Log::info(
        "Camera: {}x{}, vfov={}°, spp={}, bounces={}", image_width, image_height, settings.vfov, settings.samples_per_pixel, settings.max_bounces);

    data.lookfrom = settings.lookfrom;
    data.focus_distance = settings.focus_dist;
    data.defocus_angle = settings.defocus_angle;

    forward = glm::normalize(settings.lookat - settings.lookfrom);
    right = glm::normalize(glm::cross(forward, settings.vup));
    up = glm::cross(right, forward); // ensures orthonormal basis

    pitch = glm::degrees(std::asin(forward.y));
    yaw = glm::degrees(std::atan2(forward.z, forward.x));

    data.view = glm::lookAt(settings.lookfrom, settings.lookat, settings.vup);
    baseProjection = glm::perspective(glm::radians(settings.vfov), settings.aspect_ratio, 0.1f, 1000.0f);
    data.projection = baseProjection;
    data.inv_view = glm::inverse(data.view);
    data.inv_projection = glm::inverse(data.projection);
    data.prev_view_proj = data.projection * data.view;
}

namespace {
// Standard Halton low-discrepancy sequence. Index 0 returns 0, so callers should pass frameIndex+1.
float halton(int index, int base) {
    float f = 1.0f;
    float result = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        result += f * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}
} // namespace

void Camera::applyJitter(int frameIndex) {
    // Halton(2, 3) ∈ [0, 1)² → centered on the pixel ∈ [-0.5, 0.5)². The 0.5 scale
    // keeps samples in the inner half of each pixel, which halves silhouette wobble
    // between frames at the cost of ~2× less peak AA coverage. TAA fills the gap.
    constexpr float jitterScale = 0.5f;
    const float     jx_pix = (halton(frameIndex + 1, 2) - 0.5f) * jitterScale;
    const float     jy_pix = (halton(frameIndex + 1, 3) - 0.5f) * jitterScale;

    // Convert pixel offset to NDC. NDC spans [-1, 1] = 2 units across `image_width` pixels.
    const float jx = jx_pix * 2.0f / static_cast<float>(image_width);
    const float jy = jy_pix * 2.0f / static_cast<float>(image_height);

    // Pre-multiplying by translate(jx, jy, 0) shifts post-projection clip-space by
    // (jx*w, jy*w, 0), which after w-divide is exactly an NDC offset of (jx, jy).
    const glm::mat4 jitterMat = glm::translate(glm::mat4(1.0f), glm::vec3(jx, jy, 0.0f));
    data.projection = jitterMat * baseProjection;
    data.inv_projection = glm::inverse(data.projection);
}

Camera& Camera::update(const InputState& input, float dt) {
    // Snapshot last frame's un-jittered view*projection for temporal reprojection.
    // baseProjection is intentional: applyJitter overwrites data.projection per frame,
    // and we want motion vectors to ignore sub-pixel jitter so they describe surface
    // motion only.
    data.prev_view_proj = baseProjection * data.view;

    moving = false;

    const float speed = moveSpeed * dt;
    const float nspeed = -speed;

    if (input.moveLeft) {
        moving = true;
        translate({nspeed, 0.0f, 0.0f});
    }
    if (input.moveRight) {
        moving = true;
        translate({speed, 0.0f, 0.0f});
    }
    if (input.moveForward) {
        moving = true;
        translate({0.0f, 0.0f, speed});
    }
    if (input.moveBackward) {
        moving = true;
        translate({0.0f, 0.0f, nspeed});
    }
    if (input.moveUp) {
        moving = true;
        translate({0.0f, speed, 0.0f});
    }
    if (input.moveDown) {
        moving = true;
        translate({0.0f, nspeed, 0.0f});
    }

    const float lookDelta = lookSpeed * dt * 60.0f;
    if (input.lookLeft) {
        moving = true;
        yaw -= lookDelta;
    }
    if (input.lookRight) {
        moving = true;
        yaw += lookDelta;
    }
    if (input.lookUp) {
        moving = true;
        pitch += lookDelta;
    }
    if (input.lookDown) {
        moving = true;
        pitch -= lookDelta;
    }

    pitch = glm::clamp(pitch, -89.0f, 89.0f);
    if (moving) {
        updateDirectionVectors();
    }

    return *this;
}

void Camera::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    image_width = w;
    image_height = h;
    settings.image_width = w;
    settings.aspect_ratio = static_cast<float>(w) / static_cast<float>(h);

    baseProjection = glm::perspective(glm::radians(settings.vfov), settings.aspect_ratio, 0.1f, 1000.0f);
    data.projection = baseProjection;
    data.inv_projection = glm::inverse(data.projection);
    // Match the constructor: seed prev_view_proj with the current view*projection
    // so the very next frame's temporal reprojection can't sample against a stale
    // aspect ratio.
    data.prev_view_proj = data.projection * data.view;
}

void Camera::translate(glm::vec3 delta) {
    data.lookfrom += delta.x * right + delta.y * up + delta.z * forward;
}

void Camera::updateDirectionVectors() {
    const float radYaw = glm::radians(yaw);
    const float radPitch = glm::radians(pitch);

    forward = glm::normalize(glm::vec3(std::cos(radYaw) * std::cos(radPitch), std::sin(radPitch), std::sin(radYaw) * std::cos(radPitch)));
    right = glm::normalize(glm::cross(forward, settings.vup));
    up = glm::normalize(glm::cross(right, forward));

    updateViewMatrix();
}

void Camera::updateViewMatrix() {
    const glm::vec3 center = data.lookfrom + forward;
    data.view = glm::lookAt(data.lookfrom, center, up);
    data.inv_view = glm::inverse(data.view);
}
