#include "window.hpp"

#include "GLFW/glfw3.h"
#include "renderer.hpp"
#include "scene_graph/input_event.hpp"

namespace W3D {

void resize_callback(GLFWwindow *window, int width, int height) {
    auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    pRenderer->process_resize();
}

inline KeyCode translate_key_code(int key) {
    static const std::unordered_map<int, KeyCode> key_lookup = {{GLFW_KEY_W, KeyCode::W},
                                                                {GLFW_KEY_S, KeyCode::S},
                                                                {GLFW_KEY_A, KeyCode::A},
                                                                {GLFW_KEY_D, KeyCode::D}};
    auto it = key_lookup.find(key);
    if (it == key_lookup.end()) {
        return KeyCode::Unknown;
    } else {
        return it->second;
    }
}

inline KeyAction translate_key_action(int action) {
    if (action == GLFW_PRESS) {
        return KeyAction::Down;
    } else if (action == GLFW_RELEASE) {
        return KeyAction::Up;
    } else if (action == GLFW_REPEAT) {
        return KeyAction::Repeat;
    }
    return KeyAction::Unknown;
}

inline MouseAction translate_mouse_action(int action) {
    if (action == GLFW_PRESS) {
        return MouseAction::Down;
    } else if (action == GLFW_RELEASE) {
        return MouseAction::Up;
    }
    return MouseAction::Unknown;
}

inline MouseButton translate_mouse_button(int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        return MouseButton::Left;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        return MouseButton::Right;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        return MouseButton::Middle;
    }
    return MouseButton::Unknown;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    KeyCode key_code = translate_key_code(key);
    KeyAction key_action = translate_key_action(action);
    auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

    pRenderer->process_input_event(KeyInputEvent(key_code, key_action));
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    MouseAction mouse_action = translate_mouse_action(action);
    MouseButton mouse_button = translate_mouse_button(button);

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    pRenderer->process_input_event(MouseButtonInputEvent{
        mouse_button, mouse_action, static_cast<float>(xpos), static_cast<float>(ypos)});
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
    auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
    pRenderer->process_input_event(MouseButtonInputEvent{MouseButton::Unknown, MouseAction::Move,
                                                         static_cast<float>(xpos),
                                                         static_cast<float>(ypos)});
}

void Window::getRequiredExtensions(std::vector<const char *> &extensions) {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        extensions.push_back(*(glfwExtensions + i));
    }
};

Window::Window(const char *title, Renderer *pRenderer, int width, int height) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(handle_, pRenderer);
    glfwSetFramebufferSizeCallback(handle_, resize_callback);
    glfwSetKeyCallback(handle_, key_callback);
    glfwSetMouseButtonCallback(handle_, mouse_button_callback);
    glfwSetCursorPosCallback(handle_, cursor_position_callback);
}

Window::~Window() {
    glfwDestroyWindow(handle_);
    glfwTerminate();
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(handle_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents() {
    glfwWaitEvents();
}

void Window::getFramebufferSize(int *width, int *height) const {
    glfwGetFramebufferSize(handle_, width, height);
}

}  // namespace W3D