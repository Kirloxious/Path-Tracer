#include "window.h"
#include "log.h"
#include <GLFW/glfw3.h>

Window::Window(int width, int height, std::string_view title)
    : m_Width(width)
    , m_Height(height)
    , m_Title(title) {
    if (!glfwInit()) {
        Log::error("Failed to initialise GLFW");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_Window = glfwCreateWindow(width, height, m_Title.data(), nullptr, nullptr);
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::makeCurrentContext() {
    glfwMakeContextCurrent(m_Window);
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_Window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

InputState Window::pollInput(const KeyMappings& keys) const {
    auto pressed = [&](int key) {
        return glfwGetKey(m_Window, key) == GLFW_PRESS;
    };
    return InputState{
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
}

void Window::getFrameBufferSize() {
    glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
}