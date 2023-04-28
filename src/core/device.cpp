#include "device.hpp"

#include <memory>
#include <set>
#include <vector>

#include "instance.hpp"
#include "memory.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace W3D {
Device::Device(Instance* pInstance) {
    pInstance_ = pInstance;
    auto indices = pInstance_->queueFamilyIndices();
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.computeFamily.value(),
                                              indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    float priority = 1.0f;
    for (uint32_t familyIndex : uniqueQueueFamilies) {
        queueCreateInfos.push_back(vk::DeviceQueueCreateInfo({}, familyIndex, 1, &priority));
    }

    vk::PhysicalDeviceFeatures features;
    features.samplerAnisotropy = VK_TRUE;
    features.sampleRateShading = VK_TRUE;

    auto createInfo =
        vk::DeviceCreateInfo({}, queueCreateInfos, VALIDATION_LAYERS, DEVICE_EXTENSIONS, &features);

    device_ = vk::raii::Device(pInstance_->physicalDevice(), createInfo);
    graphicsQueue_ = vk::raii::Queue(device_, indices.graphicsFamily.value(), 0);
    presentQueue_ = vk::raii::Queue(device_, indices.presentFamily.value(), 0);
    graphicsQueue_ = vk::raii::Queue(device_, indices.computeFamily.value(), 0);

    createCommandPool();
    pAllocator_ = std::make_unique<DeviceMemory::Allocator>(*pInstance, *this);
}

void Device::createCommandPool() {
    QueueFamilyIndices indices = pInstance_->queueFamilyIndices();
    vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       indices.graphicsFamily.value());
    commandPool_ = device_.createCommandPool(poolInfo);
}

vk::raii::ImageView Device::createImageView(VkImage image, vk::Format format,
                                            vk::ImageAspectFlags aspectFlags,
                                            uint32_t mipLevels) const {
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    return device_.createImageView(viewInfo);
};

std::vector<vk::raii::CommandBuffer> Device::allocateCommandBuffers(
    vk::CommandBufferAllocateInfo& allocInfo) {
    allocInfo.commandPool = *commandPool_;
    return device_.allocateCommandBuffers(allocInfo);
}

// A Wrapper to avoid exceptions. Instead, we return proper error codes.
// For discussion, see https://github.com/KhronosGroup/Vulkan-Hpp/issues/599
vk::Result Device::presentKHR(const vk::PresentInfoKHR& presentInfo) {
    return static_cast<vk::Result>(presentQueue_.getDispatcher()->vkQueuePresentKHR(
        static_cast<VkQueue>(*presentQueue_),
        reinterpret_cast<const VkPresentInfoKHR*>(&presentInfo)));
}

void Device::transferBuffer(DeviceMemory::Buffer* src, DeviceMemory::Buffer* dst,
                            const vk::BufferCopy& copyRegion) {
    auto commandBuffer = beginOneTimeCommands();
    commandBuffer.copyBuffer(src->handle(), dst->handle(), copyRegion);
    endOneTimeCommands(commandBuffer);
}

vk::raii::CommandBuffer Device::beginOneTimeCommands() const {
    vk::CommandBufferAllocateInfo allocInfo(*commandPool_, vk::CommandBufferLevel::ePrimary, 1);
    auto commandBuffer = std::move(device_.allocateCommandBuffers(allocInfo)[0]);
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);
    return commandBuffer;
};

void Device::endOneTimeCommands(vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.end();
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(*commandBuffer);

    graphicsQueue_.submit(submitInfo);
    graphicsQueue_.waitIdle();
}

}  // namespace W3D