#pragma once

#include <memory>

#include "common/common.hpp"

namespace W3D {
namespace DeviceMemory {
class Buffer;
class Allocator;
}  // namespace DeviceMemory
class Instance;

class Device {
   public:
    Device(Instance* pInstance);
    vk::raii::ImageView createImageView(VkImage image, vk::Format format,
                                        vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) const;
    std::vector<vk::raii::CommandBuffer> allocateCommandBuffers(
        vk::CommandBufferAllocateInfo& allocInfo);
    vk::Result presentKHR(const vk::PresentInfoKHR& presentInfo);

    void transferBuffer(DeviceMemory::Buffer* src, DeviceMemory::Buffer* dst,
                        const vk::BufferCopy& copyRegion);
    vk::raii::CommandBuffer beginOneTimeCommands() const;
    void endOneTimeCommands(vk::raii::CommandBuffer& commandBuffer) const;

    vk::raii::Device* operator->() {
        return &device_;
    }
    const DeviceMemory::Allocator& get_allocator() const {
        return *pAllocator_;
    }
    const vk::raii::Device& handle() const {
        return device_;
    };
    const vk::raii::Queue& graphicsQueue() const {
        return graphicsQueue_;
    };
    const vk::raii::Queue& presentQueue() const {
        return presentQueue_;
    };
    const vk::raii::Queue& computeQueue() const {
        return computeQueue_;
    };

   private:
    void createCommandPool();
    Instance* pInstance_;
    vk::raii::Device device_ = nullptr;
    vk::raii::Queue graphicsQueue_ = nullptr;
    vk::raii::Queue presentQueue_ = nullptr;
    vk::raii::Queue computeQueue_ = nullptr;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::unique_ptr<DeviceMemory::Allocator> pAllocator_;
};
}  // namespace W3D