#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "common.hpp"
#include "device.hpp"
#include "glm/glm.hpp"
#include "instance.hpp"
#include "memory.hpp"
#include "swapchain.hpp"
#include "vulkan/vulkan_raii.hpp"

namespace W3D {
namespace gltf {
class Model;
}
class ResourceManager;
class Window;

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
    void updateUniformBuffer();
    void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    void initVulkan();
    void createRenderPass();
    vk::Format findDepthFormat();
    void createDescriptorSetLayout();
    void createDescriptorPool();

    void createFrameDatas();
    void createCommandBuffers();
    void createUniformBuffers();
    void createSyncObjects();
    void createDescriptorSets();
    void createGraphicsPipeline();
    vk::raii::ShaderModule createShaderModule(const std::string& filename);

    void recreateSwapchain();

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct FrameResource {
        vk::raii::CommandBuffer commandBuffer = nullptr;
        vk::raii::Semaphore imageAvaliableSemaphore = nullptr;
        vk::raii::Semaphore renderFinishedSemaphore = nullptr;
        vk::raii::Fence inflightFence = nullptr;
        vk::raii::DescriptorSet descriptorSet = nullptr;
        std::unique_ptr<DeviceMemory::Buffer> uniformBuffer = nullptr;
    };

    struct AttachmentResource {
        std::unique_ptr<DeviceMemory::Image> image;
        vk::raii::ImageView view;
    };

    inline const FrameResource& currentFrame() { return frameResources_[currentFrameIdx_]; }

    ResourceManager* pResourceManager_;
    Window* pWindow_;
    Instance instance_;
    Device device_;
    Swapchain swapchain_;
    DeviceMemory::Allocator allocator_;
    Config config_;
    vk::raii::RenderPass renderPass_ = nullptr;
    vk::raii::DescriptorSetLayout descriptorSetLayout_ = nullptr;
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
    uint32_t currentFrameIdx_ = 0;
    std::vector<FrameResource> frameResources_;
    std::unique_ptr<gltf::Model> pModel_;
};
}  // namespace W3D