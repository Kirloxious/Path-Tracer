#include "window.h"
#include "log.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <string>

static void glfwErrorCallback(int error, const char* description) {
    Log::error("GLFW {}: {}", error, description);
}

static constexpr const char* appId = "main";

// Hyprland (and most tiling Wayland compositors) will tile any toplevel by default,
// overriding the size requested at creation. If we're inside a Hyprland session,
// push a runtime windowrule that floats this app_id at the requested size. The rule
// lives only for the current session — nothing is written to hyprland.conf.
static void requestFloatingOnHyprland(int width, int height) {
#ifdef __linux__
    if (!std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        return;
    }
    const std::string klass = std::string("^(") + appId + ")$";
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
    Log::info("Creating window: {} x {} — '{}'", width, height, title);
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        Log::error("Failed to initialise GLFW");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // Cross-platform: these hints are no-ops on platforms where the backend doesn't match.
    glfwWindowHintString(GLFW_X11_CLASS_NAME, appId);
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, appId);
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, appId);
#endif

    requestFloatingOnHyprland(width, height);

    window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
    if (!window) {
        Log::error("Failed to create GLFW window");
        return;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Log::error("Failed to initialize GLAD");
        return;
    }
    glfwSwapInterval(0);

    Log::info("GL vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    Log::info("GL renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
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
        .debugGBufferNormal = pressed(keys.debugGBufferNormal),
        .debugGBufferPosition = pressed(keys.debugGBufferPosition),
    };

    return inputState;
}

void Window::getFrameBufferSize() {
    glfwGetFramebufferSize(window, &width, &height);
}

void Window::setTitle(const std::string& newTitle) {
    glfwSetWindowTitle(window, newTitle.c_str());
}
