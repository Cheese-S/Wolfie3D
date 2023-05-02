#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "common/common.hpp"
#include "common/glm_common.hpp"
#include "common/timer.hpp"
#include "core/descriptor_allocator.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "memory.hpp"
#include "scene_graph/scene.hpp"
#include "swapchain.hpp"
#include "window.hpp"

namespace W3D {

class InputEvent;
namespace SceneGraph {
class Node;
class FreeCamera;
class PBRMaterial;
}  // namespace SceneGraph

class Renderer {
   public:
    struct Config {
        vk::SampleCountFlagBits mssaSamples;
        int maxFramesInFlight;
    };

    Renderer(Config config = {vk::SampleCountFlagBits::e1, 2});
    ~Renderer();

    void start();
    void process_resize();
    void process_input_event(const InputEvent& input_event);

   private:
    void loop();
    void update();
    void drawFrame();

    void updateUniformBuffer();
    void recordDrawCommands(const vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);
    void draw_scene(const vk::raii::CommandBuffer& command_buffer);
    void draw_submesh(const vk::raii::CommandBuffer& command_buffer, SceneGraph::SubMesh* submesh);

    void setup_scene();
    void initVulkan();
    void createRenderPass();
    void createDescriptorSetLayout();

    void createFrameDatas();
    void createCommandBuffers();
    void createUniformBuffers();
    void createSyncObjects();
    void createDescriptorSets();
    void createGraphicsPipeline();
    vk::raii::ShaderModule createShaderModule(const std::string& filename);
    void perform_resize();

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct PushConstantObject {
        glm::mat4 model;
        glm::vec4 data;
    };

    struct FrameResource {
        vk::raii::CommandBuffer commandBuffer = nullptr;
        vk::raii::Semaphore imageAvaliableSemaphore = nullptr;
        vk::raii::Semaphore renderFinishedSemaphore = nullptr;
        vk::raii::Fence inflightFence = nullptr;
        vk::DescriptorSet descriptorSet = nullptr;
        std::unique_ptr<DeviceMemory::Buffer> uniformBuffer = nullptr;
    };

    inline const FrameResource& currentFrame() {
        return frameResources_[currentFrameIdx_];
    }

    Config config_;
    Window window_;
    Timer timer_;
    bool window_resized_ = false;

    /* ------------------------------ VULKAN STATE ------------------------------ */
    Instance instance_;
    Device device_;
    Swapchain swapchain_;
    struct DescriptorManager {
        DescriptorManager(Device& device) : allocator(&device), cache(device) {
        }
        DescriptorAllocator allocator;
        DescriptorLayoutCache cache;
        std::array<vk::DescriptorSetLayout, 2> layouts;
    } descriptor_manager_;
    vk::raii::RenderPass renderPass_ = nullptr;
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
    uint32_t currentFrameIdx_ = 0;
    std::vector<FrameResource> frameResources_;
    std::unique_ptr<SceneGraph::Scene> pScene_ = nullptr;
    SceneGraph::Node* pCamera_node;
    std::unordered_map<const SceneGraph::PBRMaterial*, vk::DescriptorSet> descriptor_set_map_;
};
}  // namespace W3D