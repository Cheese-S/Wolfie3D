#include "renderer.hpp"

#include <stdint.h>

#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "common.hpp"
#include "device.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include "memory.hpp"
#include "model.hpp"
#include "resource_manager.hpp"
#include "swapchain.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"
#include "window.hpp"

namespace W3D {
Renderer::Renderer(ResourceManager* pResourceManager, Window* pWindow, Config config)
    : config_(config),
      pResourceManager_(pResourceManager),
      pWindow_(pWindow),
      instance_(pWindow),
      device_(&instance_),
      allocator_(instance_, device_),
      swapchain_(&instance_, &device_, pWindow_, &allocator_, config_.mssaSamples) {
    initVulkan();
}

Renderer::~Renderer() { device_->waitIdle(); }

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
    // pWindow->isResized() is required since it is not guranteed that eErrorOutOfDateKHR will be
    // returned
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR || pWindow_->isResized()) {
        pWindow_->resetResizedSignal();
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
    clearValues[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.setClearValues(clearValues);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    commandBuffer.bindVertexBuffers(0, {pModel_->vertexBufferHandle()}, {0});
    commandBuffer.bindIndexBuffer(pModel_->indexBufferHandle(), 0, vk::IndexType::eUint32);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapchain_.extent().width),
                          static_cast<float>(swapchain_.extent().height), 0.0f, 1.0f);
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor({0, 0}, swapchain_.extent());
    commandBuffer.setScissor(0, scissor);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout_, 0,
                                     {*currentFrame().descriptorSet}, {});
    commandBuffer.drawIndexed(pModel_->getIndexCount(), 1, 0, 0, 0);
    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Renderer::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferObject ubo{};
    ubo.model =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ;
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swapchain_.extent().width / static_cast<float>(swapchain_.extent().height), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;  // * Flip Y coordiante so that projection works with vulkan

    auto& pUniformBuffer = currentFrame().uniformBuffer;
    if (pUniformBuffer->isHostVisible()) {
        memcpy(pUniformBuffer->mappedData(), &ubo, sizeof(ubo));
    } else {
        auto pStagingBuffer = allocator_.allocateStagingBuffer(sizeof(ubo));
        memcpy(pStagingBuffer->mappedData(), &ubo, sizeof(ubo));
        pStagingBuffer->flush();
        vk::BufferCopy copyRegion(0, 0, sizeof(ubo));
        device_.transferBuffer(pStagingBuffer.get(), pUniformBuffer.get(), copyRegion);
    }
}

void Renderer::recreateSwapchain() {
    swapchain_.recreate();
    swapchain_.createFrameBuffers(renderPass_);
}

void Renderer::initVulkan() {
    auto raw = pResourceManager_->loadGLTFModel("2.0/BoxVertexColors/glTF/BoxVertexColors.gltf");
    pModel_ = std::make_unique<gltf::Model>(raw, &device_, &allocator_);
    createRenderPass();
    createDescriptorSetLayout();
    createDescriptorPool();
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
    // vk::AttachmentReference colorAttachmentResolveRef(1, vk::ImageLayout::eAttachmentOptimal);

    // vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
    // vk::PipelineBindPoint::eGraphics,
    //                                0, nullptr, 1, &colorAttachmentRef,
    //                                &colorAttachmentResolveRef, &depthAttachmentRef);

    // vk::RenderPassCreateInfo renderPassInfo(vk::RenderPassCreateFlags(), attachmentDescriptions,
    //                                         subpass);
}

void Renderer::createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                    vk::ShaderStageFlagBits::eVertex);
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {uboLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    descriptorSetLayout_ = device_->createDescriptorSetLayout(layoutInfo);
}

void Renderer::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 1> poolSizes;
    poolSizes[0] = {vk::DescriptorType::eUniformBuffer,
                    static_cast<uint32_t>(config_.maxFramesInFlight)};

    vk::DescriptorPoolCreateInfo poolInfo({vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
                                          config_.maxFramesInFlight, poolSizes);
    descriptorPool_ = device_->createDescriptorPool(poolInfo);
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
            allocator_.allocateUniformBuffer(sizeof(UniformBufferObject));
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
    std::vector<vk::DescriptorSetLayout> layouts(config_.maxFramesInFlight, *descriptorSetLayout_);
    vk::DescriptorSetAllocateInfo allocInfo(*descriptorPool_, layouts);
    auto descriptorSets = device_->allocateDescriptorSets(allocInfo);
    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        frameResources_[i].descriptorSet = std::move(descriptorSets[i]);
        vk::DescriptorBufferInfo bufferInfo(frameResources_[i].uniformBuffer->handle(), 0,
                                            sizeof(UniformBufferObject));
        std::array<vk::WriteDescriptorSet, 1> descriptorWrites;
        descriptorWrites[0] = {*frameResources_[i].descriptorSet,  0,       0,          1,
                               vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo};
        device_->updateDescriptorSets(descriptorWrites, nullptr);
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

    auto attributeDescriptions = gltf::Vertex::attributeDescriptions();
    auto bindingDescription = gltf::Vertex::bindingDescription();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
        {}, 1, &bindingDescription, attributeDescriptions.size(), attributeDescriptions.data());

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

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 1, &(*descriptorSetLayout_));
    layout_ = device_->createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout_, *renderPass_, 0);

    pipeline_ = device_->createGraphicsPipeline(nullptr, pipelineInfo);
}

vk::raii::ShaderModule Renderer::createShaderModule(const std::string& filename) {
    auto code = pResourceManager_->loadShaderBinary(filename);
    vk::ShaderModuleCreateInfo shaderInfo(vk::ShaderModuleCreateFlags(), code.size(),
                                          reinterpret_cast<const uint32_t*>(code.data()));

    return device_->createShaderModule(shaderInfo);
}

}  // namespace W3D