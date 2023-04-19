#pragma once

#include <memory>

#include "common.hpp"
#include "vulkan/vulkan_raii.hpp"

namespace W3D {
namespace DeviceMemory {
class Buffer;
}
class Instance;

class Device {
   public:
    Device(Instance* pInstance);
    vk::raii::ImageView createImageView(VkImage image, vk::Format format,
                                        vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
    std::vector<vk::raii::CommandBuffer> allocateCommandBuffers(
        vk::CommandBufferAllocateInfo& allocInfo);
    vk::Result presentKHR(const vk::PresentInfoKHR& presentInfo);

    void transferBuffer(DeviceMemory::Buffer* src, DeviceMemory::Buffer* dst,
                        const vk::BufferCopy& copyRegion);
    vk::raii::CommandBuffer beginOneTimeCommands();
    void endOneTimeCommands(vk::raii::CommandBuffer& commandBuffer);

    vk::raii::Device* operator->() { return &device_; }
    const vk::raii::Device& handle() const { return device_; };
    const vk::raii::Queue& graphicsQueue() const { return graphicsQueue_; };
    const vk::raii::Queue& presentQueue() const { return presentQueue_; };
    const vk::raii::Queue& computeQueue() const { return computeQueue_; };

   private:
    void createCommandPool();
    Instance* pInstance_;
    vk::raii::Device device_ = nullptr;
    vk::raii::Queue graphicsQueue_ = nullptr;
    vk::raii::Queue presentQueue_ = nullptr;
    vk::raii::Queue computeQueue_ = nullptr;
    vk::raii::CommandPool commandPool_ = nullptr;
};
}  // namespace W3D