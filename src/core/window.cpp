#include "core/window.h"
#include "core/log.h"
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

Window::Window(int width, int height, std::string_view windowTitle) : width(width), height(height), title(windowTitle) {
    Log::info("Creating window: {} x {} — '{}'", width, height, title);
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        Log::error("Failed to initialise GLFW");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
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

    // Reversed-Z depth. GL's default clip range maps NDC z to [-1, 1] and then to [0, 1] in
    // the depth buffer, which wastes half the mantissa before the depth format even sees it.
    // ZERO_TO_ONE plus the near/far swap in makeReversedZProjection() puts distant geometry
    // near depth 0, where float32 is densest. GL 4.5+; this context is 4.6.
    //
    // Three things must agree or depth testing silently inverts: this call, the projection,
    // and RasterGBufferPass's GL_GREATER + 0.0 clear.
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    // Route framebuffer resize events into pending{Width,Height,Resize} so the
    // main loop can reallocate render targets between frames.
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

void Window::setTitle(std::string_view newTitle) {
    title = newTitle;
    glfwSetWindowTitle(window, title.c_str());
}
