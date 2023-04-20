#include "swapchain.hpp"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "device.hpp"
#include "instance.hpp"
#include "memory.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "window.hpp"

namespace W3D {
Swapchain::Swapchain(Instance* pInstance, Device* pDevice, Window* pWindow,
                     DeviceMemory::Allocator* pAllocator, vk::SampleCountFlagBits mssaSamples)
    : pInstance_(pInstance),
      pDevice_(pDevice),
      pWindow_(pWindow),
      pAllocator_(pAllocator),
      mssaSamples_(mssaSamples) {
    createSwapchain();
    framebuffers_.reserve(imageViews_.size());
};

void Swapchain::recreate() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        pWindow_->getFramebufferSize(&width, &height);
        pWindow_->waitEvents();
    }
    pDevice_->handle().waitIdle();
    cleanup();
    createSwapchain();
}

void Swapchain::cleanup() {
    depthResource_.view.clear();
    depthResource_.pImage.reset();
    framebuffers_.clear();
    imageViews_.clear();
    swapchain_.reset();
}

void Swapchain::createSwapchain() {
    auto details = pInstance_->swapchainSupportDetails();
    chooseSurfaceFormat(details.formats);
    choosePresentMode(details.presentModes);
    chooseExtent(details.capabilities);
    uint32_t imageCount = details.capabilities.minImageCount + 1;

    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = *(pInstance_->surface());
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat_.format;
    createInfo.imageColorSpace = surfaceFormat_.colorSpace;
    createInfo.imageExtent = extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    auto indices = pInstance_->queueFamilyIndices();
    auto uniqueQueueFamilies = pInstance_->uniqueQueueFamilies();
    auto uniqueFamilyCount = uniqueQueueFamilies.size();
    std::vector<uint32_t> uniqueIndices;
    for (auto index : uniqueQueueFamilies) {
        uniqueIndices.push_back(index);
    }

    if (uniqueFamilyCount == 1) {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = uniqueFamilyCount;
        createInfo.pQueueFamilyIndices = uniqueIndices.data();
    }
    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode_;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    swapchain_ = std::make_unique<vk::raii::SwapchainKHR>(pDevice_->handle(), createInfo);
    images_ = swapchain_->getImages();
    createImageViews();
    createDepthResource();
}

void Swapchain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    for (const auto& avaliableFormat : formats) {
        if (avaliableFormat.format == vk::Format::eB8G8R8Srgb &&
            avaliableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surfaceFormat_ = avaliableFormat;
            return;
        }
    }
    surfaceFormat_ = formats[0];
}

void Swapchain::choosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    for (const auto& avaliablePresentMode : presentModes) {
        if (avaliablePresentMode == vk::PresentModeKHR::eMailbox) {
            presentMode_ = avaliablePresentMode;
            return;
        }
    }
    presentMode_ = presentModes[0];
}

void Swapchain::chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent_ = capabilities.currentExtent;
        return;
    }

    int width, height;
    pWindow_->getFramebufferSize(&width, &height);
    vk::Extent2D actualExtent(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    extent_ = actualExtent;
}

void Swapchain::createImageViews() {
    imageViews_.reserve(images_.size());
    for (auto image : images_) {
        imageViews_.emplace_back(pDevice_->createImageView(image, surfaceFormat_.format,
                                                           vk::ImageAspectFlagBits::eColor, 1));
    }
    // for (size_t i = 0; i < images_.size(); i++) {
    //     imageViews_[i] = pDevice_->createImageView(images_[i], surfaceFormat_.format,
    //                                                vk::ImageAspectFlagBits::eColor, 1);
    // }
}

void Swapchain::createDepthResource() {
    vk::Format depthFormat = findDepthFormat();
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{extent_.width, extent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = mssaSamples_;
    depthResource_.pImage = pAllocator_->allocateAttachmentImage(imageInfo);
    depthResource_.view = pDevice_->createImageView(depthResource_.pImage->handle(), depthFormat,
                                                    vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format Swapchain::findDepthFormat() {
    std::array<vk::Format, 3> candidates = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                                            vk::Format::eD24UnormS8Uint};
    for (auto format : candidates) {
        vk::FormatProperties properties = pInstance_->physicalDevice().getFormatProperties(format);
        if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported depth format!");
}

void Swapchain::createFrameBuffers(vk::raii::RenderPass& renderPass) {
    std::array<vk::ImageView, 2> attachments;
    for (auto const& imageView : imageViews_) {
        attachments[0] = *imageView;
        attachments[1] = *depthResource_.view;
        vk::FramebufferCreateInfo framebufferCreateInfo({}, *renderPass, attachments, extent_.width,
                                                        extent_.height, 1);
        framebuffers_.emplace_back(pDevice_->handle().createFramebuffer(framebufferCreateInfo));
    }
}

// A Wrapper to avoid exceptions. Instead, we return proper error codes.
// For discussion, see https://github.com/KhronosGroup/Vulkan-Hpp/issues/599
std::pair<vk::Result, uint32_t> Swapchain::acquireNextImage(uint64_t timeout,
                                                            vk::Semaphore semaphore,
                                                            vk::Fence fence) {
    uint32_t imageIndex;
    vk::Result result = static_cast<vk::Result>(swapchain_->getDispatcher()->vkAcquireNextImageKHR(
        static_cast<VkDevice>(*pDevice_->handle()), static_cast<VkSwapchainKHR>(**swapchain_),
        timeout, static_cast<VkSemaphore>(semaphore), static_cast<VkFence>(fence), &imageIndex));
    return std::make_pair(result, imageIndex);
}

}  // namespace W3D