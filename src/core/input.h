#pragma once

/**
 * @file input.h
 * @brief API-agnostic per-frame input snapshot shared by the windowing layer and the camera.
 */

/**
 * @brief Abstract, API-agnostic snapshot of camera input actions for one frame.
 *
 * Deliberately free of any GLFW (or other windowing API) types so that Camera has no
 * dependency on the windowing layer. Produced by Window::pollInput(), consumed by
 * Camera::update().
 *
 * Every field is an edge-less *level* flag: it reports whether the action is active
 * during this frame, not whether it was just pressed.
 */
struct InputState
{
    bool moveLeft = false;
    bool moveRight = false;
    bool moveForward = false;
    bool moveBackward = false;
    bool moveUp = false;
    bool moveDown = false;
    bool lookLeft = false;
    bool lookRight = false;
    bool lookUp = false;
    bool lookDown = false;

    /// Show the G-buffer normal attachment instead of the final image (Application::run
    /// short-circuits the swap-chain blit while this is set).
    bool debugGBufferNormal = false;
    /// Show the G-buffer world-position attachment instead of the final image.
    bool debugGBufferPosition = false;
};
