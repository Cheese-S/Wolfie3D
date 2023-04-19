
#include "instance.hpp"

#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GLFW/glfw3.h"
#include "common.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "window.hpp"

#ifdef NDEBUG
constexpr const bool ENABLE_VALIDATION_LAYERS = false;
#else
constexpr const bool ENABLE_VALIDATION_LAYERS = true;
#endif

const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char*> DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Proxy function to manually load an extension function
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

bool isValidationLayerSupported() {
    auto layers = vk::enumerateInstanceLayerProperties();

    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;

        for (const auto& layerProperties : layers) {
            if (!strcmp(layerName, layerProperties.layerName)) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

namespace W3D {

Instance::Instance(Window* pWindow) {
    createInstance();
    createSurface(pWindow);
    pickPhysicalDevice();
}

void Instance::createInstance() {
    if (ENABLE_VALIDATION_LAYERS && !isValidationLayerSupported()) {
        throw std::runtime_error("validation layers requested, but not avaliable!");
    };

    vk::ApplicationInfo appInfo{};

    appInfo.pApplicationName = APP_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    vk::InstanceCreateInfo createInfo({vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR},
                                      &appInfo);

    std::vector<const char*> extensions;
    if (ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    Window::getRequiredExtensions(extensions);
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    instance_ = vk::raii::Instance(context_, createInfo);

    if (ENABLE_VALIDATION_LAYERS) {
        initDebugMessenger();
    }
}

void Instance::initDebugMessenger() {
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    populateDebugMessengerCreateInfo(debugCreateInfo);
    debugMessenger_ = vk::raii::DebugUtilsMessengerEXT(instance_, debugCreateInfo);
}

void Instance::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    using SeverityFlagBits = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageTypeBits = vk::DebugUtilsMessageTypeFlagBitsEXT;
    createInfo.messageSeverity =
        SeverityFlagBits::eInfo | SeverityFlagBits::eError | SeverityFlagBits::eVerbose;
    createInfo.messageType =
        MessageTypeBits::eGeneral | MessageTypeBits::ePerformance | MessageTypeBits::eValidation;
    createInfo.pfnUserCallback = Instance::debugCallback;
}

inline VKAPI_ATTR VkBool32 VKAPI_CALL Instance::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Instance::createSurface(Window* pWindow) {
    VkSurfaceKHR surface;
    auto window_handle = pWindow->handle();
    if (glfwCreateWindowSurface(*instance_, window_handle, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = vk::raii::SurfaceKHR(instance_, surface);
}

void Instance::pickPhysicalDevice() {
    auto physicalDevices = instance_.enumeratePhysicalDevices();
    if (!physicalDevices.size()) {
        throw std::runtime_error("failed to find GPUs with Vulkan Support");
    }

    for (auto& device : physicalDevices) {
        if (isPhysicalDeviceSuitable(device)) {
            physicalDevice_ = vk::raii::PhysicalDevice(std::move(device));
            indices_ = findQueueFamilies(physicalDevice_);
            uniqueQueueFamilies_.emplace(indices_.computeFamily.value());
            uniqueQueueFamilies_.emplace(indices_.graphicsFamily.value());
            uniqueQueueFamilies_.emplace(indices_.presentFamily.value());
            return;
        }
    }

    throw std::runtime_error("failed to find a suitable GPU!");
}

bool Instance::isPhysicalDeviceSuitable(const vk::raii::PhysicalDevice& device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool isExtensionSupported = checkPhysicalDeviceExtensionSupport(device);
    bool isSwapChainSupported = false;
    if (isExtensionSupported) {
        SwapchainSupportDetails details = queryPhysicalDeviceSwapchainSupport(device);
        isSwapChainSupported = !details.formats.empty() && !details.presentModes.empty();
    }
    auto supportedFeatures = device.getFeatures();
    return indices.isCompelete() && isExtensionSupported && isSwapChainSupported &&
           supportedFeatures.samplerAnisotropy;
}

bool Instance::checkPhysicalDeviceExtensionSupport(const vk::raii::PhysicalDevice& device) {
    auto avaliableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (const auto& extension : avaliableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

SwapchainSupportDetails Instance::queryPhysicalDeviceSwapchainSupport(
    const vk::raii::PhysicalDevice& device) const {
    SwapchainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(*surface_);
    details.formats = device.getSurfaceFormatsKHR(*surface_);
    details.presentModes = device.getSurfacePresentModesKHR(*surface_);
    return details;
}

QueueFamilyIndices Instance::findQueueFamilies(const vk::raii::PhysicalDevice& device) {
    QueueFamilyIndices indices;
    auto queueFamilies = device.getQueueFamilyProperties();
    for (size_t i = 0; i < queueFamilies.size(); i++) {
        const auto& queueFamily = queueFamilies[i];
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
            indices.computeFamily = i;
        }

        auto isPresentSupported = device.getSurfaceSupportKHR(i, *surface_);

        if (isPresentSupported) {
            indices.presentFamily = i;
        }

        if (indices.isCompelete()) {
            break;
        }
    }
    return indices;
}
}  // namespace W3D
