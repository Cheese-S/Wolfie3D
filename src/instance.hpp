#pragma once

#include <stdint.h>

#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "common.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "window.hpp"

extern const std::vector<const char*> VALIDATION_LAYERS;
extern const std::vector<const char*> DEVICE_EXTENSIONS;

namespace W3D {
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

    const vk::raii::Instance& instance() const;
    const vk::raii::PhysicalDevice& physicalDevice() const;
    const vk::raii::SurfaceKHR& surface() const;
    const SwapchainSupportDetails swapchainSupportDetails() const;
    QueueFamilyIndices queueFamilyIndices() const;
    const std::set<uint32_t>& uniqueQueueFamilies() const;

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

    std::unique_ptr<vk::raii::Context> context_;
    std::unique_ptr<vk::raii::Instance> instance_;
    std::unique_ptr<vk::raii::DebugUtilsMessengerEXT> debugMessenger_;
    std::unique_ptr<vk::raii::SurfaceKHR> surface_;
    std::unique_ptr<vk::raii::PhysicalDevice> physicalDevice_;
    QueueFamilyIndices indices_;
    std::set<uint32_t> uniqueQueueFamilies_;
};
}  // namespace W3D