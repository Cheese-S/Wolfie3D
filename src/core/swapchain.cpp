#include "swapchain.hpp"

#include "core/device_memory/image.hpp"
#include "core/instance.hpp"
#include "device.hpp"
#include "image_resource.hpp"
#include "image_view.hpp"
#include "physical_device.hpp"

namespace W3D
{

// Create a swapchain with the given extent
Swapchain::Swapchain(Device &device, vk::Extent2D window_extent) :
    device_(device)
{
	build(window_extent);
}

// Destroy all swapchain resource
Swapchain::~Swapchain()
{
	cleanup();
}

// * We only clean up what we allocated. The frame_images_ are managed by Vulkan.
void Swapchain::cleanup()
{
	p_depth_resource_.reset();
	frame_image_views_.clear();
	device_.get_handle().destroySwapchainKHR(handle_);
}

// Rebuild with new extent. We first destroy the old resource first.
void Swapchain::rebuild(vk::Extent2D new_window_extent)
{
	cleanup();
	build(new_window_extent);
}

void Swapchain::build(vk::Extent2D window_extent)
{
	const SwapchainSupportDetails &details = device_.get_physical_device().get_swapchain_support_details();
	choose_format(details.formats);
	choose_present_mode(details.present_modes);
	choose_extent(details.capabilities, window_extent);

	vk::SwapchainCreateInfoKHR swapchain_cinfo{
	    .surface          = device_.get_instance().get_surface(),
	    .minImageCount    = calc_min_image_count(details.capabilities.minImageCount, details.capabilities.maxImageCount),
	    .imageFormat      = properties_.surface_format.format,
	    .imageColorSpace  = properties_.surface_format.colorSpace,
	    .imageExtent      = properties_.extent,
	    .imageArrayLayers = 1,
	    .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
	    .preTransform     = details.capabilities.currentTransform,
	    .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
	    .presentMode      = properties_.present_mode,
	    .clipped          = true,
	    .oldSwapchain     = nullptr,
	};
	const QueueFamilyIndices &indices = device_.get_physical_device().get_queue_family_indices();

	// Our queue creation process makes no gurantee about whether present queue and graphics queue are indeed two queues.
	if (indices.graphics_index.value() == indices.present_index.value())
	{
		swapchain_cinfo.imageSharingMode = vk::SharingMode::eExclusive;
	}
	else
	{
		swapchain_cinfo.imageSharingMode      = vk::SharingMode::eConcurrent;
		swapchain_cinfo.queueFamilyIndexCount = 2;
	}

	handle_ = device_.get_handle().createSwapchainKHR(swapchain_cinfo);
	create_frame_resources();
}

// Create image views and ONE depth buffer (image + image view).
// * We don't need more than one depth buffer since we only render one frame at a time.
// * Depth buffer is no longer needed as soon as we finished rendering and are ready to present the images.
void Swapchain::create_frame_resources()
{
	frame_images_ = std::move(device_.get_handle().getSwapchainImagesKHR(handle_));

	vk::ImageViewCreateInfo swapchain_image_view_cinfo{
	    .viewType         = vk::ImageViewType::e2D,
	    .format           = properties_.surface_format.format,
	    .subresourceRange = {
	        .aspectMask     = vk::ImageAspectFlagBits::eColor,
	        .baseMipLevel   = 0,
	        .levelCount     = 1,
	        .baseArrayLayer = 0,
	        .layerCount     = 1,
	    },
	};

	for (auto image : frame_images_)
	{
		swapchain_image_view_cinfo.image = image;
		frame_image_views_.emplace_back(ImageView(device_, swapchain_image_view_cinfo));
	}

	vk::ImageCreateInfo depth_image_cinfo{

	    .imageType = vk::ImageType::e2D,
	    .format    = choose_depth_format(),
	    .extent    = vk::Extent3D{
	           .width  = properties_.extent.width,
	           .height = properties_.extent.height,
	           .depth  = 1,
        },
	    .mipLevels     = 1,
	    .arrayLayers   = 1,
	    .samples       = vk::SampleCountFlagBits::e1,
	    .tiling        = vk::ImageTiling::eOptimal,
	    .usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment,
	    .sharingMode   = vk::SharingMode::eExclusive,
	    .initialLayout = vk::ImageLayout::eUndefined,
	};
	Image img = device_.get_device_memory_allocator().allocate_device_only_image(depth_image_cinfo);

	vk::ImageViewCreateInfo depth_image_view_cinfo = ImageView::two_dim_view_cinfo(img.get_handle(), depth_image_cinfo.format, vk::ImageAspectFlagBits::eDepth, 1);
	p_depth_resource_                              = std::make_unique<ImageResource>(std::move(img), ImageView(device_, depth_image_view_cinfo));
}

// Query the physical device for candidate depth format.
vk::Format Swapchain::choose_depth_format()
{
	std::array<vk::Format, 3> candidate_formats = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};
	for (vk::Format candidate : candidate_formats)
	{
		vk::FormatProperties candidate_properties = device_.get_physical_device().get_handle().getFormatProperties(candidate);
		if (candidate_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			return candidate;
		}
	}

	throw std::runtime_error("failed to find supported depth format!");
};

// Choose surface format based on image format and color space.
void Swapchain::choose_format(const std::vector<vk::SurfaceFormatKHR> &formats)
{
	for (const auto &avaliable_surface_format : formats)
	{
		if (avaliable_surface_format.format == vk::Format::eB8G8R8Srgb && avaliable_surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			properties_.surface_format = avaliable_surface_format;
		}
	}
	properties_.surface_format = formats[0];
}

// Present mode describes how Swapchain handles present requests.
// * Mailbox is a FIFO queue that only contains one entry. For other modes, refer to spec.
void Swapchain::choose_present_mode(const std::vector<vk::PresentModeKHR> &present_modes)
{
	for (const auto &avaliable_present_mode : present_modes)
	{
		if (avaliable_present_mode == vk::PresentModeKHR::eMailbox)
		{
			properties_.present_mode = avaliable_present_mode;
			return;
		}
	}
	properties_.present_mode = present_modes[0];
}

// Choose correct extent.
void Swapchain::choose_extent(const vk::SurfaceCapabilitiesKHR &capabilities, vk::Extent2D window_extent)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		properties_.extent = capabilities.currentExtent;
		return;
	}

	// We need to clamp the extent of the images based on what surface can do.
	properties_.extent = vk::Extent2D{
	    .width  = std::clamp(window_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
	    .height = std::clamp(window_extent.height, capabilities.minImageExtent.height, capabilities.minImageExtent.height),
	};
}

// Helper function to calculate minimum image count
uint32_t Swapchain::calc_min_image_count(uint32_t min_image_count, uint32_t max_image_count)
{
	uint32_t image_count = min_image_count + 1;
	// max_image_count == 0 means there is no limit
	if (max_image_count > 0 && image_count > max_image_count)
	{
		image_count = max_image_count;
	}
	return image_count;
}

const SwapchainProperties &Swapchain::get_swapchain_properties() const
{
	return properties_;
}

const std::vector<ImageView> &Swapchain::get_frame_image_views() const
{
	return frame_image_views_;
}

const ImageResource &Swapchain::get_depth_resource() const
{
	return *p_depth_resource_;
}

}        // namespace W3D