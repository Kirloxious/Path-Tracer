#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string_view>

#include <string>

#include "core/input.h"

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

    int debugGBufferNormal = GLFW_KEY_F1;
};

/// Construction throws if GLFW, the window or GLAD fails, so a live Window always has a current 4.6 context.
class Window
{
public:
    GLFWwindow* window = nullptr;
    int         width = 0;
    int         height = 0;
    /// Owned, not a view: a scene switch reassigns the Scene::name it was built from.
    std::string title;

    /// Set by the framebuffer-size callback; the main loop drains it so targets are
    /// reallocated between frames rather than inside the callback.
    bool pendingResize = false;
    int  pendingWidth = 0;
    int  pendingHeight = 0;

    Window(int width, int height, std::string_view title);
    ~Window();

    // Also owns GLFW's global state (terminated in the destructor), so neither copy nor move is safe.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const;

    [[nodiscard]] InputState pollInput(const KeyMappings& keys = {}) const;

    void makeCurrentContext();
    void swapBuffers();
    void pollEvents();

    void getFrameBufferSize();

    void setTitle(std::string_view newTitle);
};
