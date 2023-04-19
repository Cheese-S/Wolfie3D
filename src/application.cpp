#include "application.hpp"

#include "common.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "window.hpp"

namespace W3D {

Application::Application()
    : resourceManager_(),
      window_(APP_NAME),
      renderer_(&resourceManager_, &window_, {vk::SampleCountFlagBits::e1, 2}){};

void Application::start() { loop(); }

void Application::loop() {
    while (!window_.windowShouldClose()) {
        window_.pollEvents();
        renderer_.drawFrame();
    }
}
}  // namespace W3D