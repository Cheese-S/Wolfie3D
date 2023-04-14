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
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "window.hpp"

namespace W3D {
Swapchain::Swapchain(Instance* pInstance, Device* pDevice, Window* pWindow) {
    pInstance_ = pInstance;
    pDevice_ = pDevice;
    pWindow_ = pWindow;

    createSwapchain();
};

const vk::raii::SwapchainKHR& Swapchain::handle() { return *swapchain_; };

const std::vector<vk::raii::Framebuffer>& Swapchain::framebuffers() { return framebuffers_; };

vk::Extent2D Swapchain::extent() { return extent_; };

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

void Swapchain::createFrameBuffers(vk::raii::RenderPass& renderPass) {
    framebuffers_.reserve(imageViews_.size());
    std::array<vk::ImageView, 1> attachments;
    for (auto const& imageView : imageViews_) {
        attachments[0] = *imageView;
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