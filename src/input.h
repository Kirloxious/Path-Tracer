#pragma once

// Abstract, API-agnostic snapshot of camera input actions for one frame.
// Populated by the windowing layer; consumed by the camera.
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
};
