
#include "renderer.hpp"

#include "common/error.hpp"

VKBP_DISABLE_WARNINGS()
#include <libloaderapi.h>
#include <minwindef.h>
VKBP_ENABLE_WARNINGS()

#include <renderdoc_app.h>
#include <stdint.h>

#include <array>
#include <chrono>
#include <gli/gli.hpp>
#include <memory>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "common/file_utils.hpp"
#include "common/utils.hpp"
#include "glm/gtx/string_cast.hpp"
#include "gltf_loader.hpp"
#include "memory.hpp"
#include "scene_graph/components/camera.hpp"
#include "scene_graph/components/image.hpp"
#include "scene_graph/components/mesh.hpp"
#include "scene_graph/components/pbr_material.hpp"
#include "scene_graph/components/sampler.hpp"
#include "scene_graph/components/submesh.hpp"
#include "scene_graph/components/texture.hpp"
#include "scene_graph/input_event.hpp"
#include "scene_graph/scene.hpp"
#include "scene_graph/script.hpp"

RENDERDOC_API_1_6_0* rdoc_api = NULL;

namespace W3D {

inline void set_image_layout(const vk::raii::CommandBuffer& cmd_buffer, VkImage image,
                             vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                             vk::ImageSubresourceRange subresource) {
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;
    barrier.subresourceRange = subresource;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    switch (old_layout) {
        case vk::ImageLayout::eUndefined:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            barrier.srcAccessMask = {};
            break;

        case vk::ImageLayout::eColorAttachmentOptimal:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            break;

        case vk::ImageLayout::eTransferDstOptimal:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (new_layout) {
        case vk::ImageLayout::eTransferDstOptimal:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            break;

        case vk::ImageLayout::eColorAttachmentOptimal:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (barrier.srcAccessMask == vk::AccessFlags{}) {
                barrier.srcAccessMask =
                    vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
            }
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }

    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                               vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);
}

Renderer::Renderer(Config config)
    : config_(config),
      window_(APP_NAME, this),
      instance_(&window_),
      device_(&instance_),
      swapchain_(&instance_, &device_, &window_, config_.mssaSamples),
      descriptor_manager_(device_) {
}

Renderer::~Renderer() = default;

void Renderer::process_resize() {
    window_resized_ = true;
}

void Renderer::process_input_event(const InputEvent& input_event) {
    auto scripts = pScene_->get_components<SceneGraph::Script>();
    for (auto script : scripts) {
        script->process_input_event(input_event);
    }
}

void Renderer::start() {
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
        assert(ret == 1);
        std::cout << "loaded" << std::endl;
    }
    setup_scene();
    initVulkan();
    timer_.tick();
    loop();
}

void Renderer::loop() {
    while (!window_.shouldClose()) {
        update();
        drawFrame();
        window_.pollEvents();
    }
    device_->waitIdle();
}

void Renderer::update() {
    auto delta_time = static_cast<float>(timer_.tick());
    auto scripts = pScene_->get_components<SceneGraph::Script>();
    for (auto script : scripts) {
        script->update(delta_time);
    }
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
        presentResult == vk::Result::eSuboptimalKHR || window_resized_) {
        window_resized_ = false;
        perform_resize();
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
    draw_skybox(commandBuffer);
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
    std::queue<SceneGraph::Node*> traverse_nodes;
    traverse_nodes.push(&pScene_->get_root_node());
    while (!traverse_nodes.empty()) {
        auto node = traverse_nodes.front();
        traverse_nodes.pop();
        if (node->has_component<SceneGraph::Mesh>()) {
            PushConstantObject pco;
            node->get_component<SceneGraph::Transform>().set_scale(glm::vec3(50.0f, 50.0f, 50.0f));
            pco.model = node->get_component<SceneGraph::Transform>().get_world_M();
            pco.cam_pos = pCamera_node->get_component<SceneGraph::Transform>().get_translation();
            command_buffer.pushConstants<PushConstantObject>(
                *layout_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                pco);
            auto mesh = node->get_component<SceneGraph::Mesh>();
            auto submeshes = mesh.get_submeshes();
            for (auto submesh : submeshes) {
                draw_submesh(command_buffer, submesh);
            }
        }

        auto children = node->get_children();
        for (auto child : children) {
            traverse_nodes.push(child);
        }
    }
}

void Renderer::draw_submesh(const vk::raii::CommandBuffer& command_buffer,
                            SceneGraph::SubMesh* submesh) {
    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *layout_, 1,
        {descriptor_set_map_[dynamic_cast<const SceneGraph::PBRMaterial*>(
            submesh->get_material())]},
        {});
    command_buffer.bindVertexBuffers(0, {submesh->pVertex_buffer_->handle()}, {0});
    if (submesh->pIndex_buffer_) {
        command_buffer.bindIndexBuffer(submesh->pIndex_buffer_->handle(), submesh->index_offset_,
                                       vk::IndexType::eUint32);
        command_buffer.drawIndexed(submesh->vertex_indices_, 1, 0, 0, 0);
    } else {
        command_buffer.draw(submesh->vertices_count_, 1, 0, 0);
    };
}

void Renderer::draw_skybox(const vk::raii::CommandBuffer& command_buffer) {
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *skybox_pipeline_);
    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapchain_.extent().width),
                          static_cast<float>(swapchain_.extent().height), 0.0f, 1.0f);
    command_buffer.setViewport(0, viewport);

    vk::Rect2D scissor({0, 0}, swapchain_.extent());

    command_buffer.setScissor(0, scissor);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *skybox_layout_, 0,
                                      {currentFrame().skyboxDescriptorSet}, {});
    auto meshes = pSkybox_scene_->get_components<SceneGraph::Mesh>();
    auto submesh = meshes[0]->get_submeshes()[0];
    command_buffer.bindVertexBuffers(0, {submesh->pVertex_buffer_->handle()}, {0});
    command_buffer.bindIndexBuffer(submesh->pIndex_buffer_->handle(), submesh->index_offset_,
                                   vk::IndexType::eUint32);
    command_buffer.drawIndexed(submesh->vertex_indices_, 1, 0, 0, 0);
}

void Renderer::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    auto camera = &pCamera_node->get_component<SceneGraph::Camera>();
    UniformBufferObject ubo{};
    ubo.model =
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f));  // gltf model uses up-y coordinate system.
    ubo.proj = camera->get_projection();
    ubo.view = camera->get_view();
    auto& pUniformBuffer = currentFrame().uniformBuffer;
    pUniformBuffer->update(&ubo, sizeof(ubo));
}

void Renderer::perform_resize() {
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        window_.getFramebufferSize(&width, &height);
        window_.waitEvents();
    }
    device_.handle().waitIdle();

    auto scripts = pScene_->get_components<SceneGraph::Script>();
    for (auto script : scripts) {
        script->resize(width, height);
    }
    swapchain_.recreate();
    swapchain_.createFrameBuffers(renderPass_);
}

void Renderer::initVulkan() {
    createRenderPass();
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

    vk::DescriptorSetLayoutBinding sampler_binding(0, vk::DescriptorType::eCombinedImageSampler, 1,
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
        vk::DescriptorImageInfo irradiance_info(*irradiance_.sampler,
                                                *irradiance_.texture->get_view(),
                                                vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo brdf_lut_info(*brdf_lut_.sampler, *brdf_lut_.view,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo prefilter_info(*prefilter_.sampler, *prefilter_.texture->get_view(),
                                               vk::ImageLayout::eShaderReadOnlyOptimal);
        frameResources_[i].descriptorSet =
            DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
                .bind_buffer(0, &buffer_info, vk::DescriptorType::eUniformBuffer,
                             vk::ShaderStageFlagBits::eVertex)
                .bind_image(1, &irradiance_info, vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment)
                .bind_image(2, &prefilter_info, vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment)
                .bind_image(3, &brdf_lut_info, vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment)
                .build(descriptor_manager_.layouts[0]);
        vk::DescriptorImageInfo image_info;
        image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        image_info.setImageView(*prefilter_.texture->get_view());
        image_info.setSampler(*prefilter_.sampler);
        frameResources_[i].skyboxDescriptorSet =
            DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
                .bind_buffer(0, &buffer_info, vk::DescriptorType::eUniformBuffer,
                             vk::ShaderStageFlagBits::eVertex)
                .bind_image(1, &image_info, vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment)
                .build(skybox_layouts_[0]);
    }
}

void Renderer::createGraphicsPipeline() {
    vk::raii::ShaderModule vertShaderModule = createShaderModule("shader.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("shader.frag.spv");

    vk::raii::ShaderModule skyboxVertShaderModule = createShaderModule("skybox.vert.spv");
    vk::raii::ShaderModule skyboxFragShaderModule = createShaderModule("skybox.frag.spv");

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

    vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
        sizeof(PushConstantObject));

    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, descriptor_manager_.layouts,
                                                      push_constant_range);
    layout_ = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout_, *renderPass_, 0);

    pipeline_ = device_->createGraphicsPipeline(nullptr, pipelineInfo);

    shaderStages[0].module = *skyboxVertShaderModule;
    shaderStages[1].module = *skyboxFragShaderModule;

    rasterizeInfo.cullMode = vk::CullModeFlagBits::eFront;
    depthStencil.depthWriteEnable = false;
    depthStencil.depthTestEnable = false;

    pipeline_layout_info.setSetLayouts(skybox_layouts_);
    pipeline_layout_info.setPushConstantRanges({});

    skybox_layout_ = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo skyboxPipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *skybox_layout_, *renderPass_, 0);

    skybox_pipeline_ = device_->createGraphicsPipeline(nullptr, skyboxPipelineInfo);
}

vk::raii::ShaderModule Renderer::createShaderModule(const std::string& filename) {
    auto code = fu::read_shader_binary(filename);
    vk::ShaderModuleCreateInfo shaderInfo(vk::ShaderModuleCreateFlags(), code.size(),
                                          reinterpret_cast<const uint32_t*>(code.data()));

    return device_->createShaderModule(shaderInfo);
}

void Renderer::setup_scene() {
    static const std::vector<std::string> pbr_texture_names = {
        "base_color_texture", "normal_texture", "occlusion_texture", "metallic_roughness_texture"};
    GLTFLoader loader{device_};
    pScene_ = loader.read_scene_from_file("2.0/BoomBox/glTF/BoomBox.gltf");

    auto materials = pScene_->get_components<SceneGraph::PBRMaterial>();

    for (auto material : materials) {
        auto set_builder =
            DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator);
        std::vector<vk::DescriptorImageInfo> image_infos;
        image_infos.resize(pbr_texture_names.size());

        for (int i = 0; i < pbr_texture_names.size(); i++) {
            auto& tex_name = pbr_texture_names[i];
            auto& image_info = image_infos[i];
            auto texture = material->textures_[tex_name];
            image_info.setSampler(*texture->get_sampler()->vk_sampler_);
            image_info.setImageView(*texture->get_image()->get_view());
            image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            set_builder.bind_image(i, &image_info, vk::DescriptorType::eCombinedImageSampler,
                                   vk::ShaderStageFlagBits::eFragment);
        }

        descriptor_set_map_.insert(
            std::make_pair(material, set_builder.build(descriptor_manager_.layouts[1])));
    }
    int width, height;
    window_.getFramebufferSize(&width, &height);
    pCamera_node = add_free_camera(*pScene_, "main_camera", width, height);

    auto& transform = pCamera_node->get_component<SceneGraph::Transform>();
    transform.set_tranlsation(transform.get_translation() + glm::vec3(0.0f, 0.0f, 5.0f));
    GLTFLoader sky_loader{device_};
    pSkybox_scene_ = sky_loader.read_scene_from_file("2.0/BoxTextured/glTF/BoxTextured.gltf");

    load_texture_cubemap();
}

void Renderer::load_texture_cubemap() {
    auto image = SceneGraph::Image::load_cubemap("background", "../data/texture/papermill.dds");
    image->create_vk_image(device_, vk::ImageViewType::eCube,
                           vk::ImageCreateFlagBits::eCubeCompatible);

    auto command_buffer = device_.beginOneTimeCommands();
    auto staging_bufferr = device_.get_allocator().allocateStagingBuffer(image->get_data().size());
    staging_bufferr->update(image->get_data());

    std::vector<vk::BufferImageCopy> buffer_copy_regions;

    auto& mipmaps = image->get_mipmaps();
    const auto& layers = image->get_layers();

    uint32_t offset = 0;
    for (uint32_t layer = 0; layer < layers; layer++) {
        for (size_t i = 0; i < mipmaps.size(); i++) {
            vk::BufferImageCopy buffer_copy_region;
            buffer_copy_region.imageSubresource = {vk::ImageAspectFlagBits::eColor,
                                                   static_cast<uint32_t>(i), layer, 1};
            auto extent = image->get_extent();
            buffer_copy_region.imageExtent = vk::Extent3D{extent.width >> i, extent.height >> i, 1};
            buffer_copy_region.bufferOffset = offset;
            buffer_copy_regions.push_back(buffer_copy_region);

            offset +=
                buffer_copy_region.imageExtent.width * buffer_copy_region.imageExtent.height * 16;
        }
    }

    vk::ImageSubresourceRange subresource_range(vk::ImageAspectFlagBits::eColor, 0,
                                                static_cast<uint32_t>(mipmaps.size()), 0, 6);

    vk::ImageMemoryBarrier begin_barrier;
    begin_barrier.oldLayout = vk::ImageLayout::eUndefined;
    begin_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    begin_barrier.image = image->get_vk_image().handle();
    begin_barrier.srcAccessMask = {};
    begin_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    begin_barrier.subresourceRange = subresource_range;
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                                   {begin_barrier});

    command_buffer.copyBufferToImage(staging_bufferr->handle(), image->get_vk_image().handle(),
                                     vk::ImageLayout::eTransferDstOptimal, buffer_copy_regions);

    vk::ImageMemoryBarrier end_barrier;
    end_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    end_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    end_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    end_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    end_barrier.subresourceRange = subresource_range;
    end_barrier.image = image->get_vk_image().handle();
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
                                   {end_barrier});

    device_.endOneTimeCommands(command_buffer);

    vk::SamplerCreateInfo sampler_create_info;
    sampler_create_info.magFilter = vk::Filter::eLinear;
    sampler_create_info.minFilter = vk::Filter::eLinear;
    sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.compareOp = vk::CompareOp::eNever;
    sampler_create_info.minLod = 0.0f;

    sampler_create_info.maxLod = static_cast<float>(mipmaps.size());
    sampler_create_info.maxAnisotropy =
        device_.get_instance().physical_device_properties().limits.maxSamplerAnisotropy;
    sampler_create_info.anisotropyEnable =
        device_.get_instance().physical_device_featuers().samplerAnisotropy;
    sampler_create_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    auto sampler = device_->createSampler(sampler_create_info);

    background_ = {std::move(image), std::move(sampler)};
    compute_irraidiance();
    compute_prefilter_cube();
    compute_BRDF_LUT();
}

void Renderer::compute_irraidiance() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(nullptr, nullptr);
    }
    const uint32_t DIMENSION = 64;
    /* -------------------------- setup irradiance cube ------------------------- */

    irradiance_.texture = std::make_unique<SceneGraph::Image>("irradiance");
    irradiance_.texture->set_format(vk::Format::eR32G32B32A32Sfloat);
    auto& mipmaps = irradiance_.texture->get_mut_mipmaps();
    mipmaps.push_back({0, 0, vk::Extent3D{DIMENSION, DIMENSION, 1}});
    while (true) {
        auto& last_mipmap = mipmaps.back();
        uint32_t next_width = last_mipmap.extent.width / 2;
        uint32_t next_height = last_mipmap.extent.height / 2;
        if (!next_width && !next_height) {
            break;
        }
        mipmaps.push_back(
            {last_mipmap.level + 1,
             last_mipmap.offset + last_mipmap.extent.height * last_mipmap.extent.width * 16,
             vk::Extent3D(next_width, next_height, 1)});
    }

    irradiance_.texture->create_vk_image(device_, vk::ImageViewType::eCube,
                                         vk::ImageCreateFlagBits::eCubeCompatible);

    vk::SamplerCreateInfo sampler_create_info;
    sampler_create_info.magFilter = vk::Filter::eLinear;
    sampler_create_info.minFilter = vk::Filter::eLinear;
    sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.compareOp = vk::CompareOp::eNever;
    sampler_create_info.minLod = 0.0f;

    sampler_create_info.maxLod = static_cast<float>(mipmaps.size());
    sampler_create_info.maxAnisotropy =
        device_.get_instance().physical_device_properties().limits.maxSamplerAnisotropy;
    sampler_create_info.anisotropyEnable =
        device_.get_instance().physical_device_featuers().samplerAnisotropy;
    sampler_create_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    irradiance_.sampler = device_->createSampler(sampler_create_info);

    /* ---------------------------- setup sample cube --------------------------- */

    vk::DescriptorSetLayout set_layout;
    vk::DescriptorImageInfo image_info;
    image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    image_info.setImageView(*background_.texture->get_view());
    image_info.setSampler(*background_.sampler);
    auto prefilter_descriptor =
        DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
            .bind_image(0, &image_info, vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment)
            .build(set_layout);

    /* --------------------------- setup vulkan state --------------------------- */

    vk::AttachmentDescription color_attachment;
    color_attachment.format = vk::Format::eR32G32B32A32Sfloat;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::AttachmentReference color_reference = {0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass_description;
    subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_reference;

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0] = vk::SubpassDependency(
        VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion);
    dependencies[1] = vk::SubpassDependency(
        0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion);

    vk::RenderPassCreateInfo render_pass_info({}, color_attachment, subpass_description,
                                              dependencies);
    vk::raii::RenderPass render_pass = device_->createRenderPass(render_pass_info);

    vk::ImageCreateInfo framebuffer_image_create_info;
    framebuffer_image_create_info.imageType = vk::ImageType::e2D;
    framebuffer_image_create_info.format = vk::Format::eR32G32B32A32Sfloat;
    framebuffer_image_create_info.extent = vk::Extent3D(
        64, 64, 1);  // The irrdiance map does not need high resolution. Not a lot of details.
    framebuffer_image_create_info.mipLevels = 1;
    framebuffer_image_create_info.arrayLayers = 1;
    framebuffer_image_create_info.samples = vk::SampleCountFlagBits::e1;
    framebuffer_image_create_info.tiling = vk::ImageTiling::eOptimal;
    framebuffer_image_create_info.initialLayout = vk::ImageLayout::eUndefined;
    framebuffer_image_create_info.usage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
    framebuffer_image_create_info.sharingMode = vk::SharingMode::eExclusive;
    auto framebuffer_image =
        device_.get_allocator().allocateAttachmentImage(framebuffer_image_create_info);

    vk::ImageViewCreateInfo framebuffer_view_create_info;
    framebuffer_view_create_info.viewType = vk::ImageViewType::e2D;
    framebuffer_view_create_info.format = vk::Format::eR32G32B32A32Sfloat;
    framebuffer_view_create_info.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    framebuffer_view_create_info.image = framebuffer_image->handle();

    auto framebuffer_image_view = device_->createImageView(framebuffer_view_create_info);

    vk::FramebufferCreateInfo framebuffer_create_info({}, *render_pass, *framebuffer_image_view, 64,
                                                      64, 1);
    auto framebuffer = device_->createFramebuffer(framebuffer_create_info);

    {
        auto command_buffer = device_.beginOneTimeCommands();

        set_image_layout(command_buffer, framebuffer_image->handle(), vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eColorAttachmentOptimal,
                         framebuffer_view_create_info.subresourceRange);

        device_.endOneTimeCommands(command_buffer);
    }

    vk::raii::ShaderModule vertShaderModule = createShaderModule("irradiance.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("irradiance.frag.spv");

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
    rasterizeInfo.cullMode = vk::CullModeFlagBits::eNone;
    rasterizeInfo.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({}, vk::SampleCountFlagBits::e1,
                                                             false);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        {}, false, false, vk::CompareOp::eLessOrEqual, false, false};

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

    struct IrradiancePushConstantObject {
        glm::mat4 proj;
    } ipco;

    vk::PushConstantRange push_constant_range(vk::ShaderStageFlagBits::eVertex, 0,
                                              sizeof(IrradiancePushConstantObject));

    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, set_layout, push_constant_range);
    auto layout = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout, *render_pass, 0);

    auto irradiance_pipeline = device_->createGraphicsPipeline(nullptr, pipelineInfo);

    std::array<vk::ClearValue, 1> clear_values;
    clear_values[0].color = std::array<float, 4>{0.54f, 0.81f, 0.94f, 1.0f};

    vk::RenderPassBeginInfo renderpass_begin_info(*render_pass, *framebuffer);
    renderpass_begin_info.setClearValues(clear_values);
    renderpass_begin_info.renderArea.setExtent(vk::Extent2D{DIMENSION, DIMENSION});
    auto render_cmd_buffer = device_.beginOneTimeCommands();

    std::vector<glm::mat4> matrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    vk::Viewport viewport(0, 0, static_cast<float>(DIMENSION), static_cast<float>(DIMENSION), 0, 1);
    vk::Rect2D scissor({0, 0}, {DIMENSION, DIMENSION});

    render_cmd_buffer.setViewport(0, viewport);
    render_cmd_buffer.setScissor(0, scissor);

    vk::ImageSubresourceRange cube_subresource(vk::ImageAspectFlagBits::eColor, 0,
                                               static_cast<uint32_t>(mipmaps.size()), 0, 6);

    vk::ImageMemoryBarrier begin_barrier;
    begin_barrier.oldLayout = vk::ImageLayout::eUndefined;
    begin_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    begin_barrier.image = irradiance_.texture->get_vk_image().handle();
    begin_barrier.srcAccessMask = {};
    begin_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    begin_barrier.subresourceRange = cube_subresource;
    render_cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                                      vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                                      {begin_barrier});

    for (uint32_t m = 0; m < mipmaps.size(); m++) {
        auto& mipmap = mipmaps[m];
        for (uint32_t f = 0; f < 6; f++) {
            viewport.width = mipmap.extent.width;
            viewport.height = mipmap.extent.height;
            render_cmd_buffer.setViewport(0, viewport);

            render_cmd_buffer.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);

            ipco.proj =
                glm::perspective((float)(glm::pi<float>() / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

            render_cmd_buffer.pushConstants<IrradiancePushConstantObject>(
                *layout, vk::ShaderStageFlagBits::eVertex, 0, ipco);
            render_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *irradiance_pipeline);
            render_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout, 0,
                                                 prefilter_descriptor, {});

            auto meshes = pSkybox_scene_->get_components<SceneGraph::Mesh>();
            auto submesh = meshes[0]->get_submeshes()[0];
            render_cmd_buffer.bindVertexBuffers(0, {submesh->pVertex_buffer_->handle()}, {0});
            render_cmd_buffer.bindIndexBuffer(submesh->pIndex_buffer_->handle(),
                                              submesh->index_offset_, vk::IndexType::eUint32);
            render_cmd_buffer.drawIndexed(submesh->vertex_indices_, 1, 0, 0, 0);

            render_cmd_buffer.endRenderPass();

            set_image_layout(render_cmd_buffer, framebuffer_image->handle(),
                             vk::ImageLayout::eColorAttachmentOptimal,
                             vk::ImageLayout::eTransferSrcOptimal,
                             framebuffer_view_create_info.subresourceRange);

            vk::ImageCopy copy_region;
            copy_region.srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
            copy_region.srcOffset = vk::Offset3D{0, 0, 0};
            copy_region.dstSubresource = {vk::ImageAspectFlagBits::eColor, m, f, 1};
            copy_region.dstOffset = vk::Offset3D{0, 0, 0};
            copy_region.extent = vk::Extent3D(viewport.width, viewport.height, 1);

            render_cmd_buffer.copyImage(framebuffer_image->handle(),
                                        vk::ImageLayout::eTransferSrcOptimal,
                                        irradiance_.texture->get_vk_image().handle(),
                                        vk::ImageLayout::eTransferDstOptimal, {copy_region});

            set_image_layout(render_cmd_buffer, framebuffer_image->handle(),
                             vk::ImageLayout::eTransferSrcOptimal,
                             vk::ImageLayout::eColorAttachmentOptimal,
                             framebuffer_view_create_info.subresourceRange);
        }
    }

    set_image_layout(render_cmd_buffer, irradiance_.texture->get_vk_image().handle(),
                     vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                     cube_subresource);

    device_.endOneTimeCommands(render_cmd_buffer);
    if (rdoc_api) {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}

void Renderer::compute_prefilter_cube() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(NULL, NULL);
    }
    const uint32_t DIMENSION = 512;
    /* -------------------------- setup irradiance cube ------------------------- */

    prefilter_.texture = std::make_unique<SceneGraph::Image>("irradiance");
    prefilter_.texture->set_format(vk::Format::eR16G16B16A16Sfloat);
    auto& mipmaps = prefilter_.texture->get_mut_mipmaps();
    mipmaps.push_back({0, 0, vk::Extent3D{DIMENSION, DIMENSION, 1}});
    while (true) {
        auto& last_mipmap = mipmaps.back();
        uint32_t next_width = last_mipmap.extent.width / 2;
        uint32_t next_height = last_mipmap.extent.height / 2;
        if (!next_width && !next_height) {
            break;
        }
        mipmaps.push_back(
            {last_mipmap.level + 1,
             last_mipmap.offset + last_mipmap.extent.height * last_mipmap.extent.width * 16,
             vk::Extent3D(next_width, next_height, 1)});
    }

    prefilter_.texture->create_vk_image(device_, vk::ImageViewType::eCube,
                                        vk::ImageCreateFlagBits::eCubeCompatible);

    vk::SamplerCreateInfo sampler_create_info;
    sampler_create_info.magFilter = vk::Filter::eLinear;
    sampler_create_info.minFilter = vk::Filter::eLinear;
    sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.compareOp = vk::CompareOp::eNever;
    sampler_create_info.minLod = 0.0f;

    sampler_create_info.maxLod = static_cast<float>(mipmaps.size());
    sampler_create_info.maxAnisotropy =
        device_.get_instance().physical_device_properties().limits.maxSamplerAnisotropy;
    sampler_create_info.anisotropyEnable =
        device_.get_instance().physical_device_featuers().samplerAnisotropy;
    sampler_create_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    prefilter_.sampler = device_->createSampler(sampler_create_info);

    /* ---------------------------- setup sample cube --------------------------- */

    vk::DescriptorSetLayout set_layout;
    vk::DescriptorImageInfo image_info;
    image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    image_info.setImageView(*background_.texture->get_view());
    image_info.setSampler(*background_.sampler);
    auto prefilter_descriptor =
        DescriptorBuilder::begin(&descriptor_manager_.cache, &descriptor_manager_.allocator)
            .bind_image(0, &image_info, vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment)
            .build(set_layout);

    /* --------------------------- setup vulkan state --------------------------- */

    vk::AttachmentDescription color_attachment;
    color_attachment.format = vk::Format::eR16G16B16A16Sfloat;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::AttachmentReference color_reference = {0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass_description;
    subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_reference;

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0] = vk::SubpassDependency(
        VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion);
    dependencies[1] = vk::SubpassDependency(
        0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion);

    vk::RenderPassCreateInfo render_pass_info({}, color_attachment, subpass_description,
                                              dependencies);
    vk::raii::RenderPass render_pass = device_->createRenderPass(render_pass_info);

    vk::ImageCreateInfo framebuffer_image_create_info;
    framebuffer_image_create_info.imageType = vk::ImageType::e2D;
    framebuffer_image_create_info.format = vk::Format::eR16G16B16A16Sfloat;
    framebuffer_image_create_info.extent =
        vk::Extent3D(DIMENSION, DIMENSION,
                     1);  // The irrdiance map does not need high resolution. Not a lot of details.
    framebuffer_image_create_info.mipLevels = 1;
    framebuffer_image_create_info.arrayLayers = 1;
    framebuffer_image_create_info.samples = vk::SampleCountFlagBits::e1;
    framebuffer_image_create_info.tiling = vk::ImageTiling::eOptimal;
    framebuffer_image_create_info.initialLayout = vk::ImageLayout::eUndefined;
    framebuffer_image_create_info.usage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
    framebuffer_image_create_info.sharingMode = vk::SharingMode::eExclusive;
    auto framebuffer_image =
        device_.get_allocator().allocateAttachmentImage(framebuffer_image_create_info);

    vk::ImageViewCreateInfo framebuffer_view_create_info;
    framebuffer_view_create_info.viewType = vk::ImageViewType::e2D;
    framebuffer_view_create_info.format = vk::Format::eR16G16B16A16Sfloat;
    framebuffer_view_create_info.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    framebuffer_view_create_info.image = framebuffer_image->handle();

    auto framebuffer_image_view = device_->createImageView(framebuffer_view_create_info);

    vk::FramebufferCreateInfo framebuffer_create_info({}, *render_pass, *framebuffer_image_view,
                                                      DIMENSION, DIMENSION, 1);
    auto framebuffer = device_->createFramebuffer(framebuffer_create_info);

    {
        auto command_buffer = device_.beginOneTimeCommands();

        set_image_layout(command_buffer, framebuffer_image->handle(), vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eColorAttachmentOptimal,
                         framebuffer_view_create_info.subresourceRange);

        device_.endOneTimeCommands(command_buffer);
    }

    vk::raii::ShaderModule vertShaderModule = createShaderModule("prefilter.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("prefilter.frag.spv");

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
    rasterizeInfo.cullMode = vk::CullModeFlagBits::eNone;
    rasterizeInfo.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({}, vk::SampleCountFlagBits::e1,
                                                             false);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        {}, false, false, vk::CompareOp::eLessOrEqual, false, false};

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

    struct PrefilterPushConstantObject {
        glm::mat4 proj;
        float roughness;
    } ipco;

    vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
        sizeof(PrefilterPushConstantObject));

    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, set_layout, push_constant_range);
    auto layout = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout, *render_pass, 0);

    auto prefilter_pipeline = device_->createGraphicsPipeline(nullptr, pipelineInfo);

    std::array<vk::ClearValue, 1> clear_values;
    clear_values[0].color = std::array<float, 4>{0.54f, 0.81f, 0.94f, 1.0f};

    vk::RenderPassBeginInfo renderpass_begin_info(*render_pass, *framebuffer);
    renderpass_begin_info.setClearValues(clear_values);
    renderpass_begin_info.renderArea.setExtent(vk::Extent2D{DIMENSION, DIMENSION});
    auto render_cmd_buffer = device_.beginOneTimeCommands();

    std::vector<glm::mat4> matrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                    glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    vk::Viewport viewport(0, 0, static_cast<float>(DIMENSION), static_cast<float>(DIMENSION), 0, 1);
    vk::Rect2D scissor({0, 0}, {DIMENSION, DIMENSION});

    render_cmd_buffer.setViewport(0, viewport);
    render_cmd_buffer.setScissor(0, scissor);

    vk::ImageSubresourceRange cube_subresource(vk::ImageAspectFlagBits::eColor, 0,
                                               static_cast<uint32_t>(mipmaps.size()), 0, 6);

    vk::ImageMemoryBarrier begin_barrier;
    begin_barrier.oldLayout = vk::ImageLayout::eUndefined;
    begin_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    begin_barrier.image = prefilter_.texture->get_vk_image().handle();
    begin_barrier.srcAccessMask = {};
    begin_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    begin_barrier.subresourceRange = cube_subresource;
    render_cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                                      vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                                      {begin_barrier});

    for (uint32_t m = 0; m < mipmaps.size(); m++) {
        auto& mipmap = mipmaps[m];
        ipco.roughness = (float)m / (float)(mipmaps.size() - 1);
        for (uint32_t f = 0; f < 6; f++) {
            viewport.width = mipmap.extent.width;
            viewport.height = mipmap.extent.height;
            render_cmd_buffer.setViewport(0, viewport);

            render_cmd_buffer.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);

            ipco.proj =
                glm::perspective((float)(glm::pi<float>() / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

            render_cmd_buffer.pushConstants<PrefilterPushConstantObject>(
                *layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                ipco);
            render_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *prefilter_pipeline);
            render_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout, 0,
                                                 prefilter_descriptor, {});

            auto meshes = pSkybox_scene_->get_components<SceneGraph::Mesh>();
            auto submesh = meshes[0]->get_submeshes()[0];
            render_cmd_buffer.bindVertexBuffers(0, {submesh->pVertex_buffer_->handle()}, {0});
            render_cmd_buffer.bindIndexBuffer(submesh->pIndex_buffer_->handle(),
                                              submesh->index_offset_, vk::IndexType::eUint32);
            render_cmd_buffer.drawIndexed(submesh->vertex_indices_, 1, 0, 0, 0);

            render_cmd_buffer.endRenderPass();

            set_image_layout(render_cmd_buffer, framebuffer_image->handle(),
                             vk::ImageLayout::eColorAttachmentOptimal,
                             vk::ImageLayout::eTransferSrcOptimal,
                             framebuffer_view_create_info.subresourceRange);

            vk::ImageCopy copy_region;
            copy_region.srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
            copy_region.srcOffset = vk::Offset3D{0, 0, 0};
            copy_region.dstSubresource = {vk::ImageAspectFlagBits::eColor, m, f, 1};
            copy_region.dstOffset = vk::Offset3D{0, 0, 0};
            copy_region.extent = vk::Extent3D(viewport.width, viewport.height, 1);

            render_cmd_buffer.copyImage(framebuffer_image->handle(),
                                        vk::ImageLayout::eTransferSrcOptimal,
                                        prefilter_.texture->get_vk_image().handle(),
                                        vk::ImageLayout::eTransferDstOptimal, {copy_region});

            set_image_layout(render_cmd_buffer, framebuffer_image->handle(),
                             vk::ImageLayout::eTransferSrcOptimal,
                             vk::ImageLayout::eColorAttachmentOptimal,
                             framebuffer_view_create_info.subresourceRange);
        }
    }

    set_image_layout(render_cmd_buffer, prefilter_.texture->get_vk_image().handle(),
                     vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                     cube_subresource);
    device_.endOneTimeCommands(render_cmd_buffer);
    if (rdoc_api) {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}

void Renderer::compute_BRDF_LUT() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(NULL, NULL);
    }
    const uint32_t DIMENSION = 512;
    /* -------------------------- setup irradiance cube ------------------------- */
    vk::ImageCreateInfo image_create_info;
    image_create_info.imageType = vk::ImageType::e2D;
    image_create_info.format = vk::Format::eR16G16Sfloat;
    image_create_info.extent = vk::Extent3D(DIMENSION, DIMENSION, 1);
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = vk::SampleCountFlagBits::e1;
    image_create_info.tiling = vk::ImageTiling::eOptimal;
    image_create_info.usage =
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    brdf_lut_.image = device_.get_allocator().allocateAttachmentImage(image_create_info);

    vk::ImageViewCreateInfo image_view_create_info;
    image_view_create_info.viewType = vk::ImageViewType::e2D;
    image_view_create_info.image = brdf_lut_.image->handle();
    image_view_create_info.format = vk::Format::eR16G16Sfloat;
    image_view_create_info.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    brdf_lut_.view = device_->createImageView(image_view_create_info);

    vk::SamplerCreateInfo sampler_create_info;
    sampler_create_info.magFilter = vk::Filter::eLinear;
    sampler_create_info.minFilter = vk::Filter::eLinear;
    sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.compareOp = vk::CompareOp::eNever;
    sampler_create_info.minLod = 0.0f;

    sampler_create_info.maxLod = static_cast<float>(1);
    sampler_create_info.maxAnisotropy =
        device_.get_instance().physical_device_properties().limits.maxSamplerAnisotropy;
    sampler_create_info.anisotropyEnable =
        device_.get_instance().physical_device_featuers().samplerAnisotropy;
    sampler_create_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    brdf_lut_.sampler = device_->createSampler(sampler_create_info);

    /* --------------------------- setup vulkan state --------------------------- */

    vk::AttachmentDescription color_attachment;
    color_attachment.format = vk::Format::eR16G16Sfloat;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::AttachmentReference color_reference = {0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass_description;
    subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_reference;

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0] = vk::SubpassDependency(
        VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion);
    dependencies[1] = vk::SubpassDependency(
        0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion);

    vk::RenderPassCreateInfo render_pass_info({}, color_attachment, subpass_description,
                                              dependencies);
    vk::raii::RenderPass render_pass = device_->createRenderPass(render_pass_info);

    vk::FramebufferCreateInfo framebuffer_create_info({}, *render_pass, *brdf_lut_.view, DIMENSION,
                                                      DIMENSION, 1);
    auto framebuffer = device_->createFramebuffer(framebuffer_create_info);

    vk::ImageSubresourceRange subresource(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    {
        auto command_buffer = device_.beginOneTimeCommands();

        set_image_layout(command_buffer, brdf_lut_.image->handle(), vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eColorAttachmentOptimal, subresource);

        device_.endOneTimeCommands(command_buffer);
    }

    vk::raii::ShaderModule vertShaderModule = createShaderModule("brdf_lut.vert.spv");
    vk::raii::ShaderModule fragShaderModule = createShaderModule("brdf_lut.frag.spv");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex,
                                                          *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment,
                                                          *fragShaderModule, "main");
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo,
                                                                  fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, {}, {});

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
        {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::PipelineViewportStateCreateInfo viewportStateInfo({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizeInfo;
    rasterizeInfo.depthClampEnable = false;
    rasterizeInfo.depthBiasEnable = false;
    rasterizeInfo.rasterizerDiscardEnable = false;
    rasterizeInfo.polygonMode = vk::PolygonMode::eFill;
    rasterizeInfo.lineWidth = 1.0f;
    rasterizeInfo.cullMode = vk::CullModeFlagBits::eNone;
    rasterizeInfo.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({}, vk::SampleCountFlagBits::e1,
                                                             false);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        {}, false, false, vk::CompareOp::eLessOrEqual, false, false};

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

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    auto layout = device_->createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {}, 2, shaderStages.data(), &vertexInputInfo, &inputAssemblyInfo, nullptr,
        &viewportStateInfo, &rasterizeInfo, &multisamplingInfo, &depthStencil, &colorBlending,
        &dynamicStateInfo, *layout, *render_pass, 0);

    auto brdf_lut_pipeline = device_->createGraphicsPipeline(nullptr, pipelineInfo);

    std::array<vk::ClearValue, 1> clear_values;
    clear_values[0].color = std::array<float, 4>{0.54f, 0.81f, 0.94f, 1.0f};

    vk::RenderPassBeginInfo renderpass_begin_info(*render_pass, *framebuffer);
    renderpass_begin_info.setClearValues(clear_values);
    renderpass_begin_info.renderArea.setExtent(vk::Extent2D{DIMENSION, DIMENSION});
    auto render_cmd_buffer = device_.beginOneTimeCommands();

    vk::Viewport viewport(0, 0, static_cast<float>(DIMENSION), static_cast<float>(DIMENSION), 0, 1);
    vk::Rect2D scissor({0, 0}, {DIMENSION, DIMENSION});

    render_cmd_buffer.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);
    render_cmd_buffer.setViewport(0, viewport);
    render_cmd_buffer.setScissor(0, scissor);
    render_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *brdf_lut_pipeline);
    render_cmd_buffer.draw(3, 1, 0, 0);
    render_cmd_buffer.endRenderPass();

    device_.graphicsQueue().waitIdle();

    device_.endOneTimeCommands(render_cmd_buffer);
    std::cout << "hjere" << std::endl;
    if (rdoc_api) {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}
}  // namespace W3D
