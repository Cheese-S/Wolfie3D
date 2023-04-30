#include "renderer.hpp"

#include <stdint.h>

#include <array>
#include <chrono>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gltf_loader.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "common/file_utils.hpp"
#include "memory.hpp"
#include "scene_graph/components/image.hpp"
#include "scene_graph/components/mesh.hpp"
#include "scene_graph/components/sampler.hpp"
#include "scene_graph/components/submesh.hpp"
#include "scene_graph/components/texture.hpp"
#include "scene_graph/scene.hpp"

namespace W3D {
Renderer::Renderer(Config config)
    : config_(config),
      window_(APP_NAME),
      instance_(&window_),
      device_(&instance_),
      swapchain_(&instance_, &device_, &window_, config_.mssaSamples),
      descriptor_manager_(device_) {
}

Renderer::~Renderer() = default;

void Renderer::start() {
    initVulkan();
    loadModels();
    loop();
}

void Renderer::loop() {
    while (!window_.shouldClose()) {
        updateTime();
        drawFrame();
        window_.pollEvents();
    }
    device_->waitIdle();
}

void Renderer::updateTime() {
    float currentFrameTime = window_.getTime();
    time_.deltaTime = currentFrameTime - time_.lastFrameTime;
    time_.lastFrameTime = currentFrameTime;
}

void Renderer::drawFrame() {
    const auto& device_handle_ = device_.handle();
    const auto& currentFrame = frameResources_[currentFrameIdx_];
    while (vk::Result::eTimeout ==
           device_handle_.waitForFences({*currentFrame.inflightFence}, true, UINT64_MAX))
        ;

    auto [result, imageIndex] =
        swapchain_.acquireNextImage(UINT64_MAX, *currentFrame.imageAvaliableSemaphore, nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        swapchain_.recreate();
    }
    updateUniformBuffer();
    device_handle_.resetFences({*currentFrame.inflightFence});
    currentFrame.commandBuffer.reset();
    recordDrawCommands(currentFrame.commandBuffer, imageIndex);

    vk::SubmitInfo submitInfo;
    std::array<vk::Semaphore, 1> waitSemaphores{*currentFrame.imageAvaliableSemaphore};
    vk::PipelineStageFlags waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.pWaitDstStageMask = &waitStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(*currentFrame.commandBuffer);

    std::array<vk::Semaphore, 1> signalSemaphores{*currentFrame.renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    device_.graphicsQueue().submit({submitInfo}, *currentFrame.inflightFence);

    std::array<vk::SwapchainKHR, 1> swapchains = {*swapchain_.handle()};
    std::array<uint32_t, 1> imageIndices = {imageIndex};
    vk::PresentInfoKHR presentInfo(signalSemaphores, swapchains, imageIndices);

    auto presentResult = device_.presentKHR(presentInfo);
    // pWindow->isResized() is required since it is not guranteed that eErrorOutOfDateKHR will
    // be returned
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR || window_.isResized()) {
        window_.resetResizedSignal();
        recreateSwapchain();
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image");
    }
    currentFrameIdx_ = (currentFrameIdx_ + 1) % config_.maxFramesInFlight;
}

void Renderer::recordDrawCommands(const vk::raii::CommandBuffer& commandBuffer,
                                  uint32_t imageIndex) {
    vk::CommandBufferBeginInfo beginInfo;

    commandBuffer.begin(beginInfo);

    const auto& framebuffers = swapchain_.framebuffers();
    vk::RenderPassBeginInfo renderPassInfo(*renderPass_, *framebuffers[imageIndex],
                                           {{0, 0}, swapchain_.extent()});

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = std::array<float, 4>{0.54f, 0.81f, 0.94f, 1.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.setClearValues(clearValues);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapchain_.extent().width),
                          static_cast<float>(swapchain_.extent().height), 0.0f, 1.0f);
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor({0, 0}, swapchain_.extent());
    commandBuffer.setScissor(0, scissor);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout_, 0,
                                     {currentFrame().descriptorSet}, {});
    draw_scene(commandBuffer);

    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Renderer::draw_scene(const vk::raii::CommandBuffer& command_buffer) {
    auto submeshes = pScene_->get_components<SceneGraph::SubMesh>();
    for (auto submesh : submeshes) {
        auto set = descriptor_set_map_[dynamic_cast<const SceneGraph::PBRMaterial*>(
            submesh->get_material())];
        command_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *layout_, 1,
            {descriptor_set_map_[dynamic_cast<const SceneGraph::PBRMaterial*>(
                submesh->get_material())]},
            {});
        command_buffer.bindVertexBuffers(0, {submesh->pVertex_buffer_->handle()}, {0});
        if (submesh->pIndex_buffer_) {
            command_buffer.bindIndexBuffer(submesh->pIndex_buffer_->handle(),
                                           submesh->index_offset_, vk::IndexType::eUint32);
            command_buffer.drawIndexed(submesh->vertex_indices_, 1, 0, 0, 0);
        } else {
            command_buffer.draw(submesh->vertices_count_, 1, 0, 0);
        };
    }
}

void Renderer::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferObject ubo{};
    ubo.model =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swapchain_.extent().width / static_cast<float>(swapchain_.extent().height), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    auto& pUniformBuffer = currentFrame().uniformBuffer;
    pUniformBuffer->update(&ubo, sizeof(ubo));
}

void Renderer::recreateSwapchain() {
    swapchain_.recreate();
    swapchain_.createFrameBuffers(renderPass_);
}

void Renderer::initVulkan() {
    createRenderPass();
    createDescriptorSetLayout();
    swapchain_.createFrameBuffers(renderPass_);
    createFrameDatas();
    createGraphicsPipeline();
}

void Renderer::createRenderPass() {
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = swapchain_.imageFormat();
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depthAttachment;
    depthAttachment.format = swapchain_.findDepthFormat();
    depthAttachment.samples = config_.mssaSamples;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference depthAttachmentRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = {};
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                               vk::AccessFlagBits::eColorAttachmentWrite |
                               vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo({}, 2, attachments.data(), 1, &subpass, 1, &dependency);
    renderPass_ = device_->createRenderPass(renderPassInfo);

    // vk::AttachmentDescription colorAttachmentResolve;
    // colorAttachmentResolve.format = swapchain_.imageFormat();
    // colorAttachmentResolve.samples = config_.mssaSamples;
    // colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
    // colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
    // colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    // colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    // colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
    // colorAttachmentResolve.finalLayout = vk::ImageLayout::ePresentSrcKHR;
    // vk::AttachmentReference colorAttachmentResolveRef(1,
    // vk::ImageLayout::eAttachmentOptimal);

    // vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
    // vk::PipelineBindPoint::eGraphics,
    //                                0, nullptr, 1, &colorAttachmentRef,
    //                                &colorAttachmentResolveRef, &depthAttachmentRef);

    // vk::RenderPassCreateInfo renderPassInfo(vk::RenderPassCreateFlags(),
    // attachmentDescriptions,
    //                                         subpass);
}

void Renderer::createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding ubo_layout_binding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                      vk::ShaderStageFlagBits::eVertex);
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {ubo_layout_binding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    descriptor_manager_.layouts[0] =
        descriptor_manager_.cache.create_descriptor_layout(&layoutInfo);

    vk::DescriptorSetLayoutBinding sampler_binding(1, vk::DescriptorType::eCombinedImageSampler, 1,
                                                   vk::ShaderStageFlagBits::eFragment);
    bindings[0] = sampler_binding;
    descriptor_manager_.layouts[1] =
        descriptor_manager_.cache.create_descriptor_layout(&layoutInfo);
}

void Renderer::createFrameDatas() {
    frameResources_.resize(config_.maxFramesInFlight);
    createCommandBuffers();
    createUniformBuffers();
    createSyncObjects();
    createDescriptorSets();
}

void Renderer::createCommandBuffers() {
    vk::CommandBufferAllocateInfo commandBufferAllocInfo;
    commandBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    commandBufferAllocInfo.commandBufferCount = config_.maxFramesInFlight;
    std::vector<vk::raii::CommandBuffer> commandBuffers =
        device_.allocateCommandBuffers(commandBufferAllocInfo);
    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        frameResources_[i].commandBuffer = std::move(commandBuffers[i]);
    }
}

void Renderer::createUniformBuffers() {
    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        frameResources_[i].uniformBuffer =
            device_.get_allocator().allocateUniformBuffer(sizeof(UniformBufferObject));
    }
}

void Renderer::createSyncObjects() {
    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);
    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        frameResources_[i].imageAvaliableSemaphore = device_->createSemaphore(semaphoreInfo);
        frameResources_[i].renderFinishedSemaphore = device_->createSemaphore(semaphoreInfo);
        frameResources_[i].inflightFence = device_->createFence(fenceInfo);
    }
}

void Renderer::createDescriptorSets() {
    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        vk::DescriptorBufferInfo buffer_info(frameResources_[i].uniformBuffer->handle(), 0,
                                             sizeof(UniformBufferObject));
        frameResources_[i].descriptorSet =
            DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
                .bind_buffer(0, &buffer_info, vk::DescriptorType::eUniformBuffer,
                             vk::ShaderStageFlagBits::eVertex)
                .build(descriptor_manager_.layouts[0]);
    }
}

void Renderer::createGraphicsPipeline() {
    vk::raii::ShaderModule vertShaderModule = createShaderModule("shader.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("shader.frag.spv");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex,
                                                          *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment,
                                                          *fragShaderModule, "main");
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo,
                                                                  fragShaderStageInfo};

    std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions;
    attribute_descriptions[0] = {0, 0, vk::Format::eR32G32B32Sfloat,
                                 offsetof(SceneGraph::Vertex, pos)};
    attribute_descriptions[1] = {1, 0, vk::Format::eR32G32B32Sfloat,
                                 offsetof(SceneGraph::Vertex, norm)};
    attribute_descriptions[2] = {2, 0, vk::Format::eR32G32Sfloat, offsetof(SceneGraph::Vertex, uv)};

    std::array<vk::VertexInputBindingDescription, 1> binding_descriptions;
    binding_descriptions[0] = vk::VertexInputBindingDescription(0, sizeof(SceneGraph::Vertex),
                                                                vk::VertexInputRate::eVertex);
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, binding_descriptions,
                                                           attribute_descriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
        {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::PipelineViewportStateCreateInfo viewportStateInfo({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizeInfo;
    rasterizeInfo.depthClampEnable = false;
    rasterizeInfo.depthBiasEnable = false;
    rasterizeInfo.rasterizerDiscardEnable = false;
    rasterizeInfo.polygonMode = vk::PolygonMode::eFill;
    rasterizeInfo.lineWidth = 1.0f;
    rasterizeInfo.cullMode = vk::CullModeFlagBits::eBack;
    rasterizeInfo.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({}, vk::SampleCountFlagBits::e1,
                                                             false);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{{},    true, true, vk::CompareOp::eLess,
                                                         false, false};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable = false;
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = false;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<vk::DynamicState> dynamicStates{vk::DynamicState::eViewport,
                                                vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, dynamicStates);

    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, descriptor_manager_.layouts);
    layout_ = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout_, *renderPass_, 0);

    pipeline_ = device_->createGraphicsPipeline(nullptr, pipelineInfo);
}

vk::raii::ShaderModule Renderer::createShaderModule(const std::string& filename) {
    auto code = fu::read_shader_binary(filename);
    vk::ShaderModuleCreateInfo shaderInfo(vk::ShaderModuleCreateFlags(), code.size(),
                                          reinterpret_cast<const uint32_t*>(code.data()));

    return device_->createShaderModule(shaderInfo);
}

void Renderer::loadModels() {
    GLTFLoader loader{device_};
    pScene_ = loader.read_scene_from_file("2.0/BoxTextured/glTF/BoxTextured.gltf");

    auto materials = pScene_->get_components<SceneGraph::PBRMaterial>();

    for (auto material : materials) {
        vk::DescriptorImageInfo image_info;
        auto texture = material->textures_["base_color_texture"];
        image_info.setSampler(*texture->get_sampler()->vk_sampler_);
        image_info.setImageView(*texture->get_image()->get_view());
        image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto set =
            DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
                .bind_image(1, &image_info, vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment)
                .build(descriptor_manager_.layouts[1]);
        descriptor_set_map_.insert(std::make_pair(material, set));
    }
}

}  // namespace W3D