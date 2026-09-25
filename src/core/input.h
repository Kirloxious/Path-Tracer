#pragma once

/// Level flags: whether each action is held this frame, not whether it was just pressed.
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

    bool debugGBufferNormal = false;
};
