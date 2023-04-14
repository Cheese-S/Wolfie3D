#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "GLFW/glfw3.h"

const int DEFAULT_WIDTH = 800;
const int DEFAULT_HEIGHT = 600;

namespace W3D {
class Window {
   public:
    Window(const char* title, int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT);
    ~Window();
    static void getRequiredExtensions(std::vector<const char*>& extensions);
    bool windowShouldClose();
    void pollEvents();
    void waitEvents();
    GLFWwindow* handle();
    void getFramebufferSize(int* width, int* height) const;

    inline bool isResized() {
        bool queryResult = isResized_;
        return queryResult;
    }

    inline void resetResizedSignal() { isResized_ = false; }

   private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* handle_;
    bool isResized_;
};
}  // namespace W3D