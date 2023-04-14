#pragma once

#include <memory>

#include "common.hpp"
#include "instance.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace W3D {
class Device {
   public:
    Device(Instance* pInstance);
    vk::raii::ImageView createImageView(VkImage image, vk::Format format,
                                        vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
    std::vector<vk::raii::CommandBuffer> allocateCommandBuffers(
        vk::CommandBufferAllocateInfo& allocInfo);
    vk::Result presentKHR(const vk::PresentInfoKHR& presentInfo);

    const vk::raii::Device& handle() const;
    const vk::raii::Queue& graphicsQueue() const;
    const vk::raii::Queue& presentQueue() const;
    const vk::raii::Queue& computeQueue() const;

   private:
    void createCommandPool();
    Instance* pInstance_;
    std::unique_ptr<vk::raii::Device> device_;
    std::unique_ptr<vk::raii::Queue> graphicsQueue_;
    std::unique_ptr<vk::raii::Queue> presentQueue_;
    std::unique_ptr<vk::raii::Queue> computeQueue_;
    vk::raii::CommandPool commandPool_ = nullptr;
};
}  // namespace W3D