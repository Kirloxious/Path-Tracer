#include "window.h"
#include "log.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <string>

static void glfwErrorCallback(int error, const char* description) {
    Log::error("GLFW {}: {}", error, description);
}

static constexpr const char* kAppId = "main";

// Hyprland (and most tiling Wayland compositors) will tile any toplevel by default,
// overriding the size requested at creation. If we're inside a Hyprland session,
// push a runtime windowrule that floats this app_id at the requested size. The rule
// lives only for the current session — nothing is written to hyprland.conf.
static void requestFloatingOnHyprland(int width, int height) {
#ifdef __linux__
    if (!std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        return;
    }
    const std::string klass = std::string("^(") + kAppId + ")$";
    const std::string floatRule = "hyprctl keyword windowrulev2 'float, class:" + klass + "' >/dev/null 2>&1";
    const std::string sizeRule =
        "hyprctl keyword windowrulev2 'size " + std::to_string(width) + " " + std::to_string(height) + ", class:" + klass + "' >/dev/null 2>&1";
    std::system(floatRule.c_str());
    std::system(sizeRule.c_str());
#else
    (void)width;
    (void)height;
#endif
}

Window::Window(int width, int height, std::string_view title) : width(width), height(height), title(title) {
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        Log::error("Failed to initialise GLFW");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Cross-platform: these hints are no-ops on platforms where the backend doesn't match.
    glfwWindowHintString(GLFW_X11_CLASS_NAME, kAppId);
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, kAppId);
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, kAppId);
#endif

    requestFloatingOnHyprland(width, height);

    window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
}

Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::makeCurrentContext() {
    glfwMakeContextCurrent(window);
}

void Window::swapBuffers() {
    glfwSwapBuffers(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

InputState Window::pollInput(const KeyMappings& keys) const {
    auto pressed = [&](int key) {
        return glfwGetKey(window, key) == GLFW_PRESS;
    };

    auto inputState = InputState{
        .moveLeft = pressed(keys.moveLeft),
        .moveRight = pressed(keys.moveRight),
        .moveForward = pressed(keys.moveForward),
        .moveBackward = pressed(keys.moveBackward),
        .moveUp = pressed(keys.moveUp),
        .moveDown = pressed(keys.moveDown),
        .lookLeft = pressed(keys.lookLeft),
        .lookRight = pressed(keys.lookRight),
        .lookUp = pressed(keys.lookUp),
        .lookDown = pressed(keys.lookDown),
    };

    return inputState;
}

void Window::getFrameBufferSize() {
    glfwGetFramebufferSize(window, &width, &height);
}
