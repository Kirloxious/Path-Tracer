#include "scene/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <cmath>

#include "core/log.h"

namespace {
// Reversed-Z puts distant geometry near depth 0, where float32 is densest, making depth-based
// position reconstruction usable. Requires ZERO_TO_ONE clip control, GL_GREATER and a 0.0 clear.
glm::mat4 makeReversedZProjection(const CameraSettings& settings) {
    constexpr float nearPlane = 0.1f;
    constexpr float farPlane = 1000.0f;
    return glm::perspectiveZO(glm::radians(settings.vfov), settings.aspect_ratio, farPlane, nearPlane);
}
} // namespace

Camera::Camera(const CameraSettings& settings)
    : settings(settings), image_width(settings.image_width), image_height(static_cast<int>(settings.image_width / settings.aspect_ratio)) {

    Log::info("Camera: {}x{}, vfov={}°, bounces={}", image_width, image_height, settings.vfov, settings.max_bounces);

    data.lookfrom = settings.lookfrom;
    data.focus_distance = settings.focus_dist;
    data.defocus_angle = settings.defocus_angle;

    forward = glm::normalize(settings.lookat - settings.lookfrom);
    right = glm::normalize(glm::cross(forward, settings.vup));
    up = glm::cross(right, forward);

    pitch = glm::degrees(std::asin(forward.y));
    yaw = glm::degrees(std::atan2(forward.z, forward.x));

    data.view = glm::lookAt(settings.lookfrom, settings.lookat, settings.vup);
    baseProjection = makeReversedZProjection(settings);
    data.projection = baseProjection;
    data.inv_view = glm::inverse(data.view);
    data.inv_projection = glm::inverse(data.projection);
    data.prev_view_proj = data.projection * data.view;
}

namespace {
// Index 0 returns 0, so callers should pass frameIndex + 1.
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
    // Scaled to the inner half of the pixel to halve silhouette wobble; TAA covers the lost AA reach.
    constexpr float jitterScale = 0.5f;
    const float     jx_pix = (halton(frameIndex + 1, 2) - 0.5f) * jitterScale;
    const float     jy_pix = (halton(frameIndex + 1, 3) - 0.5f) * jitterScale;

    const float jx = jx_pix * 2.0f / static_cast<float>(image_width);
    const float jy = jy_pix * 2.0f / static_cast<float>(image_height);

    const glm::mat4 jitterMat = glm::translate(glm::mat4(1.0f), glm::vec3(jx, jy, 0.0f));
    data.projection = jitterMat * baseProjection;
    data.inv_projection = glm::inverse(data.projection);
}

void Camera::update(const InputState& input, float dt) {
    // Un-jittered, so motion vectors describe surface motion only.
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
}

void Camera::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    image_width = w;
    image_height = h;
    settings.image_width = w;
    settings.aspect_ratio = static_cast<float>(w) / static_cast<float>(h);

    baseProjection = makeReversedZProjection(settings);
    data.projection = baseProjection;
    data.inv_projection = glm::inverse(data.projection);
    // Reseeded so the next frame's reprojection can't use a stale aspect ratio.
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
