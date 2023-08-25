#pragma once

#include "common/vk_common.hpp"
#include "core/device_memory/allocator.hpp"
#include "core/device_memory/device_memory_object.hpp"

namespace W3D
{

template <typename T>
class Key;

class Image : public DeviceMemoryObject<vk::Image>
{
  public:
	Image(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, vk::ImageCreateInfo &image_cinfo, VmaAllocationCreateInfo &allocation_cinfo);
	~Image();
	Image(Image &&rhs);

	vk::Extent3D get_base_extent();

  private:
	vk::Extent3D base_extent_;
};
}        // namespace W3D