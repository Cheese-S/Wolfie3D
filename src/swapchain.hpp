#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "common.hpp"
#include "vulkan/vulkan_enums.hpp"

namespace W3D {
namespace DeviceMemory {
class Allocator;
class Image;
};  // namespace DeviceMemory
class Instance;
class Device;
class Window;

class Swapchain {
   public:
    Swapchain(Instance* pInstance, Device* pDevice, Window* pWindow,
              DeviceMemory::Allocator* pAllocator, vk::SampleCountFlagBits mssaSamples);

    void recreate();

    std::pair<vk::Result, uint32_t> acquireNextImage(uint64_t timeout, vk::Semaphore semaphore,
                                                     vk::Fence fence);

    inline vk::Format imageFormat() { return surfaceFormat_.format; };
    const vk::raii::SwapchainKHR& handle() { return *swapchain_; }
    const std::vector<vk::raii::Framebuffer>& framebuffers() { return framebuffers_; }
    vk::Extent2D extent() { return extent_; };
    void createFrameBuffers(vk::raii::RenderPass& renderPass);
    vk::Format findDepthFormat();

   private:
    void createSwapchain();
    void chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
    void choosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
    void chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
    void createImageViews();
    void createColorResources();
    void createDepthResource();
    void cleanup();

    struct AttachmentResource {
        std::unique_ptr<DeviceMemory::Image> pImage = nullptr;
        vk::raii::ImageView view = nullptr;
    };

    Window* pWindow_;
    Instance* pInstance_;
    Device* pDevice_;
    DeviceMemory::Allocator* pAllocator_;
    vk::SampleCountFlagBits mssaSamples_;
    vk::SurfaceFormatKHR surfaceFormat_;
    vk::PresentModeKHR presentMode_;
    vk::Extent2D extent_;
    std::unique_ptr<vk::raii::SwapchainKHR> swapchain_;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    std::vector<vk::raii::Framebuffer> framebuffers_;
    AttachmentResource depthResource_;
};
}  // namespace W3D