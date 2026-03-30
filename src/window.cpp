#include "window.h"
#include "log.h"
#include <GLFW/glfw3.h>

static void glfwErrorCallback(int error, const char* description) {
    Log::error("GLFW {}: {}", error, description);
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