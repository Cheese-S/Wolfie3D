#pragma once

#include "common/vk_common.hpp"
#include "core/device_memory/allocator.hpp"
#include "core/device_memory/device_memory_object.hpp"

namespace W3D
{

template <typename T>
class Key;

// RAII wrapper for vkImage and its memory.
// * All images in our application need to be allocated through the allocator.
class Image : public DeviceMemoryObject<vk::Image>
{
  public:
	Image(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, std::nullptr_t nptr);
	Image(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, vk::ImageCreateInfo &image_cinfo, VmaAllocationCreateInfo &allocation_cinfo);
	~Image();
	Image(Image &&rhs);
	Image &operator=(Image &&);

	vk::Extent3D get_base_extent();
	vk::Format   get_format();

  private:
	vk::Extent3D base_extent_;
	vk::Format   format_;
};
}        // namespace W3D