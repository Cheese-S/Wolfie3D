#pragma once

#include "renderer.hpp"
#include "resource_manager.hpp"
#include "window.hpp"

namespace W3D {
class Application {
   public:
    Application();
    void start();

   private:
    void loop();
    ResourceManager resourceManager_;
    Window window_;
    Renderer renderer_;
};
}  // namespace W3D