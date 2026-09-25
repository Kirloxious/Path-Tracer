#include "core/window.h"
#include "core/log.h"
#include "gpu/gl.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <stdexcept>
#include <string>

static void glfwErrorCallback(int error, const char* description) {
    Log::error("GLFW {}: {}", error, description);
}

static constexpr const char* appId = "main";

// Hyprland tiles every toplevel, overriding the requested size; push a session-only rule
// that floats this app_id at that size instead.
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

Window::Window(int width, int height, std::string_view windowTitle) : width(width), height(height), title(windowTitle) {
    Log::info("Creating window: {} x {} — '{}'", width, height, title);
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialise GLFW");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // No-ops on backends that don't match.
    glfwWindowHintString(GLFW_X11_CLASS_NAME, appId);
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, appId);
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, appId);
#endif

    requestFloatingOnHyprland(width, height);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window (is OpenGL 4.6 available?)");
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to load OpenGL entry points (GLAD)");
    }
    glfwSwapInterval(0);

    // Reversed-Z: must agree with makeReversedZProjection() and RasterGBufferPass's GL_GREATER
    // + 0.0 clear, or depth testing silently inverts.
    GL::setClipDepthZeroToOne();

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int fbW, int fbH) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self || fbW <= 0 || fbH <= 0) {
            return;
        }
        self->pendingResize = true;
        self->pendingWidth = fbW;
        self->pendingHeight = fbH;
    });

    const GL::ContextInfo info = GL::contextInfo();
    Log::info("OpenGL {} — {} ({})", info.version, info.renderer, info.vendor);
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
    };

    return inputState;
}

void Window::getFrameBufferSize() {
    glfwGetFramebufferSize(window, &width, &height);
}

void Window::setTitle(std::string_view newTitle) {
    title = newTitle;
    glfwSetWindowTitle(window, title.c_str());
}
