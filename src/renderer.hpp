#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "device.hpp"
#include "instance.hpp"
#include "resource_manager.hpp"
#include "swapchain.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "window.hpp"

namespace W3D {
class Renderer {
   public:
    struct Config {
        vk::SampleCountFlagBits mssaSamples;
        int maxFramesInFlight;
    };
    Renderer(ResourceManager* pResourceManager, Window* pWindow, Config config);
    ~Renderer();
    void drawFrame();

   private:
    void recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    void initVulkan();
    void createRenderPass();
    vk::Format findDepthFormat();

    void createGraphicsPipeline();
    vk::raii::ShaderModule createShaderModule(const std::string& filename);

    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapchain();

    ResourceManager* pResourceManager_;
    Window* pWindow_;
    Instance instance_;
    Device device_;
    Swapchain swapchain_;
    Config config_;
    vk::raii::RenderPass renderPass_ = nullptr;
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> imageAvaliableSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> inflightFences_;
    uint32_t currentFrame_ = 0;
};
}  // namespace W3D