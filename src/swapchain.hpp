#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "common.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "window.hpp"

namespace W3D {
class Swapchain {
   public:
    Swapchain(Instance* pInstance, Device* pDevice, Window* pWindow);

    void recreate();
    void createSwapchain();
    void createImageViews();
    void createColorResources();
    void createDepthResources();
    void createFrameBuffers(vk::raii::RenderPass& renderPass);

    std::pair<vk::Result, uint32_t> acquireNextImage(uint64_t timeout, vk::Semaphore semaphore,
                                                     vk::Fence fence);

    inline vk::Format imageFormat() { return surfaceFormat_.format; };
    const vk::raii::SwapchainKHR& handle();
    const std::vector<vk::raii::Framebuffer>& framebuffers();
    vk::Extent2D extent();

   private:
    void chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
    void choosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
    void chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    void cleanup();

    Instance* pInstance_;
    Device* pDevice_;
    Window* pWindow_;
    std::unique_ptr<vk::raii::SwapchainKHR> swapchain_;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    std::vector<vk::raii::Framebuffer> framebuffers_;
    vk::SurfaceFormatKHR surfaceFormat_;
    vk::PresentModeKHR presentMode_;
    vk::Extent2D extent_;
};
}  // namespace W3D