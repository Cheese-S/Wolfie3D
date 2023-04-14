#include "renderer.hpp"

#include <stdint.h>

#include <array>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "common.hpp"
#include "device.hpp"
#include "swapchain.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace W3D {
Renderer::Renderer(ResourceManager* pResourceManager, Window* pWindow, Config config)
    : pResourceManager_(pResourceManager),
      pWindow_(pWindow),
      instance_(pWindow),
      device_(&instance_),
      swapchain_(&instance_, &device_, pWindow_),
      config_(config) {
    initVulkan();
}

Renderer::~Renderer() { device_.handle().waitIdle(); }

void Renderer::drawFrame() {
    const auto& device_handle_ = device_.handle();
    while (vk::Result::eTimeout ==
           device_handle_.waitForFences({*inflightFences_[currentFrame_]}, true, UINT64_MAX))
        ;

    auto [result, imageIndex] =
        swapchain_.acquireNextImage(UINT64_MAX, *imageAvaliableSemaphores_[currentFrame_], nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        swapchain_.recreate();
    }
    device_handle_.resetFences({*inflightFences_[currentFrame_]});
    commandBuffers_[currentFrame_].reset();
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    vk::SubmitInfo submitInfo;
    std::array<vk::Semaphore, 1> waitSemaphores{*imageAvaliableSemaphores_[currentFrame_]};
    vk::PipelineStageFlags waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = &waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(*commandBuffers_[currentFrame_]);

    std::array<vk::Semaphore, 1> signalSemaphores{*renderFinishedSemaphores_[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    device_.graphicsQueue().submit({submitInfo}, *inflightFences_[currentFrame_]);

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
    currentFrame_ = (currentFrame_ + 1) % config_.maxFramesInFlight;
}

void Renderer::recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex) {
    vk::CommandBufferBeginInfo beginInfo;

    commandBuffer.begin(beginInfo);

    const auto& framebuffers = swapchain_.framebuffers();
    vk::RenderPassBeginInfo renderPassInfo(*renderPass_, *framebuffers[imageIndex],
                                           {{0, 0}, swapchain_.extent()});

    vk::ClearValue clearColor(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.setClearValues(clearColor);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapchain_.extent().width),
                          static_cast<float>(swapchain_.extent().height), 0.0f, 1.0f);
    commandBuffer.setViewport(0, viewport);
    vk::Rect2D scissor({0, 0}, swapchain_.extent());
    commandBuffer.setScissor(0, scissor);
    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Renderer::recreateSwapchain() {
    swapchain_.recreate();
    swapchain_.createFrameBuffers(renderPass_);
}

void Renderer::initVulkan() {
    createRenderPass();
    createGraphicsPipeline();
    swapchain_.createFrameBuffers(renderPass_);
    createCommandBuffers();
    createSyncObjects();
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

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = {};
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass, 1, &dependency);
    renderPass_ = device_.handle().createRenderPass(renderPassInfo);

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

    // vk::AttachmentDescription depthAttachment;
    // depthAttachment.format = findDepthFormat();
    // depthAttachment.samples = config_.mssaSamples;
    // depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    // depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    // depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    // depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    // depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
    // depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    // vk::AttachmentReference depthAttachmentRef(2, vk::ImageLayout::eDepthAttachmentOptimal);

    // vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
    // vk::PipelineBindPoint::eGraphics,
    //                                0, nullptr, 1, &colorAttachmentRef,
    //                                &colorAttachmentResolveRef, &depthAttachmentRef);

    // std::array<vk::AttachmentDescription, 3> attachmentDescriptions = {
    //     colorAttachment, colorAttachmentResolve, depthAttachment};

    // vk::RenderPassCreateInfo renderPassInfo(vk::RenderPassCreateFlags(), attachmentDescriptions,
    //                                         subpass);
}

vk::Format Renderer::findDepthFormat() {
    std::array<vk::Format, 3> candidates = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                                            vk::Format::eD24UnormS8Uint};
    for (auto format : candidates) {
        vk::FormatProperties properties = instance_.physicalDevice().getFormatProperties(format);
        if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported depth format!");
}

void Renderer::createGraphicsPipeline() {
    vk::raii::ShaderModule vertShaderModule = createShaderModule("shaders/shader.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("shaders/shader.frag.spv");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex,
                                                          *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment,
                                                          *fragShaderModule, "main");
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo,
                                                                  fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0, nullptr);

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
    rasterizeInfo.frontFace = vk::FrontFace::eClockwise;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({}, vk::SampleCountFlagBits::e1,
                                                             false);

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

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 0, 0);
    layout_ = device_.handle().createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, nullptr, &colorBlending,
        &dynamicStateInfo, *layout_, *renderPass_, 0);

    pipeline_ = device_.handle().createGraphicsPipeline(nullptr, pipelineInfo);
}

vk::raii::ShaderModule Renderer::createShaderModule(const std::string& filename) {
    auto code = pResourceManager_->readFile(filename);
    vk::ShaderModuleCreateInfo shaderInfo(vk::ShaderModuleCreateFlags(), code.size(),
                                          reinterpret_cast<const uint32_t*>(code.data()));
    return device_.handle().createShaderModule(shaderInfo);
}

void Renderer::createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = config_.maxFramesInFlight;
    commandBuffers_ = device_.allocateCommandBuffers(allocInfo);
}

void Renderer::createSyncObjects() {
    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (int i = 0; i < config_.maxFramesInFlight; i++) {
        inflightFences_.push_back(device_.handle().createFence(fenceInfo));
        renderFinishedSemaphores_.push_back(device_.handle().createSemaphore(semaphoreInfo));
        imageAvaliableSemaphores_.push_back(device_.handle().createSemaphore(semaphoreInfo));
    }
}

}  // namespace W3D