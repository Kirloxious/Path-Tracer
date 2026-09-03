#pragma once

/**
 * @file window.h
 * @brief GLFW window + GL context ownership and the input translation layer.
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string_view>

#include <string>

#include "core/input.h"

/**
 * @brief GLFW key codes used to populate an InputState in Window::pollInput().
 *
 * Passed by value with a default-constructed fallback, so callers that want the stock
 * WASD + arrows binding can simply call `pollInput()`.
 */
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

/**
 * @brief Owns the GLFW window and its OpenGL 4.6 core context.
 *
 * Construction failure is reported through Log::error and leaves `window` null rather than
 * throwing — callers should check `window` before using the context.
 */
class Window
{
public:
    GLFWwindow* window = nullptr;
    int         width = 0;
    int         height = 0;
    /// Owned, not a view: Application constructs the Window from Scene::name, and a scene
    /// switch reassigns that string. A string_view here dangled from the second scene on.
    std::string title;

    /// Set by the GLFW framebuffer-size callback whenever the OS resizes the window.
    /// Application drains this each frame to reallocate render targets on the render
    /// thread rather than inside the callback.
    bool pendingResize = false;
    int  pendingWidth = 0;
    int  pendingHeight = 0;

    /**
     * @brief Initialises GLFW, creates the window and makes its GL 4.6 context current.
     *
     * Requests a debug context in non-NDEBUG builds. On Hyprland it pushes a session-only
     * `hyprctl` window rule so the window floats at the requested size; nothing is written
     * to hyprland.conf.
     *
     * @param width       Requested framebuffer width in pixels.
     * @param height      Requested framebuffer height in pixels.
     * @param title       Window title; copied into `title`.
     */
    Window(int width, int height, std::string_view title);
    ~Window();

    // Owns the GLFW window *and* the library's global state, which the destructor tears down.
    // Neither survives being duplicated or handed off, so copy and move are both gone.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// @return true once the user has closed the window or the context failed to create.
    [[nodiscard]] bool shouldClose() const;

    /**
     * @brief Translates the current GLFW key state into a windowing-API-agnostic snapshot.
     *
     * @param keys Key bindings to sample; defaults to the stock WASD + arrows layout.
     * @return One frame's worth of action flags for Camera::update().
     */
    [[nodiscard]] InputState pollInput(const KeyMappings& keys = {}) const;

    /// Makes this window's GL context current on the calling thread.
    void makeCurrentContext();
    /// Presents the back buffer.
    void swapBuffers();
    /// Drains the GLFW event queue, which may set `pendingResize`.
    void pollEvents();

    /// Re-reads the framebuffer size from GLFW into `width` / `height`.
    void getFrameBufferSize();

    /**
     * @brief Replaces the window title.
     * @param newTitle Copied into `title`; the caller's buffer need not outlive the call.
     */
    void setTitle(std::string_view newTitle);
};
