#include "framebuffer.hpp"

#include "device.hpp"
#include "device_memory/image.hpp"
#include "image_resource.hpp"
#include "image_view.hpp"
#include "render_pass.hpp"
#include "swapchain.hpp"

namespace W3D
{

// create vkFramebuffer with the given create info
Framebuffer::Framebuffer(Device &device, vk::FramebufferCreateInfo framebuffer_cinfo) :
    device_(device)
{
	handle_ = device_.get_handle().createFramebuffer(framebuffer_cinfo);
}

// Cleanup.
Framebuffer::~Framebuffer()
{
	device_.get_handle().destroyFramebuffer(handle_);
}

// Build the framebuffers with a given swapchain and a given renderpass.
SwapchainFramebuffer::SwapchainFramebuffer(Device &device, Swapchain &swapchain, RenderPass &render_pass) :
    device_(device),
    swapchain_(swapchain),
    render_pass_(render_pass)
{
	build();
}

// Cleanup
SwapchainFramebuffer::~SwapchainFramebuffer()
{
	cleanup();
}

// We only need to take care of the framebuffers. The imageviews are managed by the swapchain.
void SwapchainFramebuffer::cleanup()
{
	for (vk::Framebuffer framebuffer : framebuffers_)
	{
		device_.get_handle().destroyFramebuffer(framebuffer);
	}
	framebuffers_.clear();
}

void SwapchainFramebuffer::rebuild()
{
	cleanup();
	build();
}

// Create the framebuffers.
void SwapchainFramebuffer::build()
{
	// We use the color image views and depth image view from the swapchain.
	const std::vector<ImageView> &frame_image_views = swapchain_.get_frame_image_views();
	const ImageView              &depth_image_view  = swapchain_.get_depth_resource().get_view();
	vk::Extent2D                  extent            = swapchain_.get_swapchain_properties().extent;

	// Use the same depth image for every framebuffer.
	// *We synchronize rendring so that we don't accidentally overwrite the depth buffer.
	std::array<vk::ImageView, 2> attachments;
	vk::FramebufferCreateInfo    framebuffer_cinfo{
	       .renderPass      = render_pass_.get_handle(),
	       .attachmentCount = to_u32(attachments.size()),
	       .pAttachments    = attachments.data(),
	       .width           = extent.width,
	       .height          = extent.height,
	       .layers          = 1,
    };
	for (auto const &frame_image_view : frame_image_views)
	{
		attachments[0] = frame_image_view.get_handle();
		attachments[1] = depth_image_view.get_handle();
		framebuffers_.push_back(device_.get_handle().createFramebuffer(framebuffer_cinfo));
	}
}

const std::vector<vk::Framebuffer> &SwapchainFramebuffer::get_handles() const
{
	return framebuffers_;
}

const vk::Framebuffer &SwapchainFramebuffer::get_handle(uint32_t idx) const
{
	return framebuffers_[idx];
}
}        // namespace W3D