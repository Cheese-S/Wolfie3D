#pragma once

#include "common/vk_common.hpp"
#include "core/vulkan_object.hpp"

namespace W3D
{
class Device;
class Swapchain;

// Wrapper class for VkRenderPass
// * Renderpass is a concept only exists in Vulkan. It DESCRIBES a set of image resources used during rendering and subpasses (stpes in a renderpass).
// * It is merely a meta data object that gives hint to the GPU.
class RenderPass : public VulkanObject<vk::RenderPass>
{
  public:
	static vk::AttachmentDescription color_attachment(vk::Format format, vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined, vk::ImageLayout final_layout = vk::ImageLayout::eColorAttachmentOptimal);

	static vk::AttachmentDescription depth_attachment(vk::Format format, vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined, vk::ImageLayout final_layout = vk::ImageLayout::eDepthAttachmentOptimal);

	RenderPass(Device &device, std::nullptr_t nptr);
	RenderPass(Device &device, vk::RenderPassCreateInfo render_pass_cinfo);
	~RenderPass() override;

  private:
	Device &device_;
};
}        // namespace W3D