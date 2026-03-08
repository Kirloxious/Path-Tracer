#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(const CameraSettings& settings)
    : settings(settings), image_width(settings.image_width), image_height(static_cast<int>(settings.image_width / settings.aspect_ratio)) {

    data.lookfrom = settings.lookfrom;
    data.focus_distance = settings.focus_dist;
    data.defocus_angle = settings.defocus_angle;

    forward = glm::normalize(settings.lookat - settings.lookfrom);
    right = glm::normalize(glm::cross(forward, settings.vup));
    up = glm::cross(right, forward); // ensures orthonormal basis

    pitch = glm::degrees(std::asin(forward.y));
    yaw = glm::degrees(std::atan2(forward.z, forward.x));

    data.view = glm::lookAt(settings.lookfrom, settings.lookat, settings.vup);
    data.projection = glm::perspective(glm::radians(settings.vfov), settings.aspect_ratio, 0.1f, 1000.0f);
    data.inv_view = glm::inverse(data.view);
    data.inv_projection = glm::inverse(data.projection);
}

Camera& Camera::update(const InputState& input, float dt) {
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
