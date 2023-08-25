#include "image.hpp"

#include "common/error.hpp"
#include "core/image_view.hpp"

namespace W3D
{

Image::Image(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, vk::ImageCreateInfo &image_cinfo, VmaAllocationCreateInfo &allocation_cinfo) :
    DeviceMemoryObject(allocator, key),
    base_extent_(image_cinfo.extent)
{
	details_.allocator = allocator;
	VkImage c_image_handle;
	VK_CHECK(vmaCreateImage(details_.allocator, reinterpret_cast<VkImageCreateInfo *>(&image_cinfo), &allocation_cinfo, &c_image_handle, &details_.allocation, &details_.allocation_info));
	handle_      = c_image_handle;
	base_extent_ = image_cinfo.extent;
}

Image::~Image()
{
	if (handle_)
	{
		vmaDestroyImage(details_.allocator, handle_, details_.allocation);
	}
}

Image::Image(Image &&rhs) :
    DeviceMemoryObject(std::move(rhs)),
    base_extent_(rhs.base_extent_)
{
}

vk::Extent3D Image::get_base_extent()
{
	return base_extent_;
}

}        // namespace W3D