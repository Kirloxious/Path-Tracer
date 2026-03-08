#pragma once

#include <GLFW/glfw3.h>
#include <string_view>

#include "input.h"

// GLFW key bindings used to populate an InputState via Window::pollInput().
struct KeyMappings
{
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int moveForward = GLFW_KEY_W;
    int moveBackward = GLFW_KEY_S;
    int moveUp = GLFW_KEY_SPACE;
    int moveDown = GLFW_KEY_LEFT_CONTROL;
    int lookLeft = GLFW_KEY_LEFT;
    int lookRight = GLFW_KEY_RIGHT;
    int lookUp = GLFW_KEY_UP;
    int lookDown = GLFW_KEY_DOWN;
};

class Window
{
public:
    GLFWwindow*      window = nullptr;
    int              width = 0;
    int              height = 0;
    std::string_view title;

    Window(int width, int height, std::string_view title);
    ~Window();

    [[nodiscard]] bool       shouldClose() const;
    [[nodiscard]] InputState pollInput(const KeyMappings& keys = {}) const;

    void makeCurrentContext();
    void swapBuffers();
    void pollEvents();
    void getFrameBufferSize();
};
