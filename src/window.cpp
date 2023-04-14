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
    glfwSetFramebufferSizeCallback(handle_, framebufferResizeCallback);
}

void Window::framebufferResizeCallback(GLFWwindow *handle, int width, int height) {
    auto pWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(handle));
    pWindow->isResized_ = true;
}

Window::~Window() {
    glfwDestroyWindow(handle_);
    glfwTerminate();
}

GLFWwindow *Window::handle() { return handle_; }

bool Window::windowShouldClose() { return glfwWindowShouldClose(handle_); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::waitEvents() { glfwWaitEvents(); }

void Window::getFramebufferSize(int *width, int *height) const {
    glfwGetFramebufferSize(handle_, width, height);
}

}  // namespace W3D