#include "window.hpp"

#include "GLFW/glfw3.h"
namespace W3D {
void Window::getRequiredExtensions(std::vector<const char *> &extensions) {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        extensions.push_back(*(glfwExtensions + i));
    }
};

Window::Window(const char *title, int width, int height) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, glfwFramebufferResizeCallback);
}

Window::~Window() {
    glfwDestroyWindow(handle_);
    glfwTerminate();
}

void Window::registerCursorCallback(CursorCallback callback) {
    cursorCallback_ = callback;
    glfwSetCursorPosCallback(handle_, glfwCursorMovementCallback);
}

void Window::registerScrollCallback(ScrollCallback callback) {
    scrollCallback_ = callback;
    glfwSetScrollCallback(handle_, glfwScrollCallback);
}

void Window::glfwFramebufferResizeCallback(GLFWwindow *handle, int width, int height) {
    auto pWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(handle));
    pWindow->isResized_ = true;
}

void Window::glfwScrollCallback(GLFWwindow *handle, double xoffset, double yoffset) {
    auto pWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(handle));
    pWindow->scrollCallback_(xoffset, yoffset);
}

void Window::glfwCursorMovementCallback(GLFWwindow *handle, double xposIn, double yposIn) {
    auto pWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(handle));
    if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        // pWindow->cursorCallback_(xposIn, yposIn);
    }
}

bool Window::shouldClose() { return glfwWindowShouldClose(handle_); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::waitEvents() { glfwWaitEvents(); }

void Window::getFramebufferSize(int *width, int *height) const {
    glfwGetFramebufferSize(handle_, width, height);
}

}  // namespace W3D