#pragma once

#include <stdint.h>

#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "common/common.hpp"

extern const std::vector<const char*> VALIDATION_LAYERS;
extern const std::vector<const char*> DEVICE_EXTENSIONS;

namespace W3D {
class Window;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool isCompelete() {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
};

struct SwapchainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Instance {
   public:
    Instance(Window* pWindow);
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    void createInstance();

    const vk::raii::Instance& handle() const {
        return instance_;
    };
    const vk::raii::PhysicalDevice& physicalDevice() const {
        return physicalDevice_;
    };
    const vk::raii::SurfaceKHR& surface() const {
        return surface_;
    };
    QueueFamilyIndices queueFamilyIndices() const {
        return indices_;
    };
    const std::set<uint32_t>& uniqueQueueFamilies() const {
        return uniqueQueueFamilies_;
    };
    const SwapchainSupportDetails swapchainSupportDetails() const {
        return queryPhysicalDeviceSwapchainSupport(physicalDevice_);
    };
    const vk::PhysicalDeviceProperties& physical_device_properties() const {
        return properties_;
    }
    const vk::PhysicalDeviceFeatures& physical_device_featuers() const {
        return features_;
    }

   private:
    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);
    void initDebugMessenger();
    inline static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    void pickPhysicalDevice();
    void createSurface(Window* pWindow);
    bool isPhysicalDeviceSuitable(const vk::raii::PhysicalDevice& device);
    bool checkPhysicalDeviceExtensionSupport(const vk::raii::PhysicalDevice& device);
    QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device);
    SwapchainSupportDetails queryPhysicalDeviceSwapchainSupport(
        const vk::raii::PhysicalDevice& device) const;

    vk::raii::Context context_;
    vk::raii::Instance instance_ = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::SurfaceKHR surface_ = nullptr;
    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::PhysicalDeviceProperties properties_;
    vk::PhysicalDeviceFeatures features_;
    QueueFamilyIndices indices_;
    std::set<uint32_t> uniqueQueueFamilies_;
};
}  // namespace W3D