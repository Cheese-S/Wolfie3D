#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

#include "GLFW/glfw3.h"

const int DEFAULT_WIDTH = 800;
const int DEFAULT_HEIGHT = 600;

namespace W3D {
class Application;
class Window {
   public:
    Window(const char* title, int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT);
    ~Window();
    static void getRequiredExtensions(std::vector<const char*>& extensions);
    bool shouldClose();
    void pollEvents();
    void waitEvents();
    GLFWwindow* handle() { return handle_; };
    void getFramebufferSize(int* width, int* height) const;
    inline float getTime() { return static_cast<float>(glfwGetTime()); }

    using CursorCallback = std::function<void(double, double)>;
    using ScrollCallback = std::function<void(double, double)>;

    void registerCursorCallback(CursorCallback callback);
    void registerScrollCallback(ScrollCallback callback);

    inline bool isResized() {
        bool queryResult = isResized_;
        return queryResult;
    }

    inline void resetResizedSignal() { isResized_ = false; }

   private:
    static void glfwFramebufferResizeCallback(GLFWwindow* handle, int width, int height);
    static void glfwScrollCallback(GLFWwindow* handle, double xoffset, double yoffset);
    static void glfwCursorMovementCallback(GLFWwindow* handle, double xposIn, double yposIn);
    ScrollCallback scrollCallback_;
    CursorCallback cursorCallback_;
    Application* pApplication;
    GLFWwindow* handle_;
    bool isResized_;
};
}  // namespace W3D