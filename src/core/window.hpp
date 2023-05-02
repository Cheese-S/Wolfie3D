#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

#include "GLFW/glfw3.h"

const int DEFAULT_WIDTH = 800;
const int DEFAULT_HEIGHT = 600;

namespace W3D {
class Renderer;
class Window {
   public:
    Window(const char* title, Renderer* pRenderer, int width = DEFAULT_WIDTH,
           int height = DEFAULT_HEIGHT);
    ~Window();
    static void getRequiredExtensions(std::vector<const char*>& extensions);
    bool shouldClose();
    void pollEvents();
    void waitEvents();
    GLFWwindow* handle() {
        return handle_;
    };
    void getFramebufferSize(int* width, int* height) const;
    inline float getTime() {
        return static_cast<float>(glfwGetTime());
    }

   private:
    GLFWwindow* handle_;
};
}  // namespace W3D