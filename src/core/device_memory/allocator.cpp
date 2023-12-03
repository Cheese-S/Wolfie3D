#include "allocator.hpp"

#include "common/utils.hpp"
#include "core/device.hpp"
#include "core/instance.hpp"
#include "core/physical_device.hpp"

#include "buffer.hpp"
#include "image.hpp"

namespace W3D
{

// Create the VMA allocator.
DeviceMemoryAllocator::DeviceMemoryAllocator(Device &device)
{
	const Instance        &instance        = device.get_instance();
	const PhysicalDevice  &physical_device = device.get_physical_device();
	VmaAllocatorCreateInfo allocator_cinfo{
	    .physicalDevice   = physical_device.get_handle(),
	    .device           = device.get_handle(),
	    .instance         = instance.get_handle(),
	    .vulkanApiVersion = VK_API_VERSION_1_2,
	};

	vmaCreateAllocator(&allocator_cinfo, &handle_);
}

DeviceMemoryAllocator::~DeviceMemoryAllocator()
{
	vmaDestroyAllocator(handle_);
}

// All hints (allocation_cinfo.flags & allocation_cinfo.usage) are recommended by VMA.
// * Refer to https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html

// Allocate a staging buffer.
// * A staging buffer is a mapped buffer that we can directly write to.
// * We then perform GPU-side copy by using it as the src.
Buffer DeviceMemoryAllocator::allocate_staging_buffer(size_t size) const
{
	vk::BufferCreateInfo buffer_cinfo{};
	buffer_cinfo.size  = size;
	buffer_cinfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	VmaAllocationCreateInfo allocation_cinfo{};
	allocation_cinfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocation_cinfo.usage = VMA_MEMORY_USAGE_AUTO;
	return allocate_buffer(buffer_cinfo, allocation_cinfo);
}

// Allocate a vertex buffer.
// * A vertex buffer contains vertex information.
Buffer DeviceMemoryAllocator::allocate_vertex_buffer(size_t size) const
{
	vk::BufferCreateInfo buffer_cinfo{};
	buffer_cinfo.size  = size;
	buffer_cinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	VmaAllocationCreateInfo allocation_cinfo{};
	allocation_cinfo.flags = 0;
	allocation_cinfo.usage = VMA_MEMORY_USAGE_AUTO;
	return allocate_buffer(buffer_cinfo, allocation_cinfo);
}

// Allocate an index buffer.
// * An index buffer contains index information.
Buffer DeviceMemoryAllocator::allocate_index_buffer(size_t size) const
{
	vk::BufferCreateInfo buffer_cinfo{};
	buffer_cinfo.size  = size;
	buffer_cinfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	VmaAllocationCreateInfo allocation_cinfo{};
	allocation_cinfo.flags = 0;
	allocation_cinfo.usage = VMA_MEMORY_USAGE_AUTO;
	return allocate_buffer(buffer_cinfo, allocation_cinfo);
}

// Allocate an uniform buffer.
// * W3D uses a mapped uniform buffer so that it can be updated easily.
Buffer DeviceMemoryAllocator::allocate_uniform_buffer(size_t size) const
{
	vk::BufferCreateInfo buffer_cinfo{};
	buffer_cinfo.size  = size;
	buffer_cinfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
	VmaAllocationCreateInfo allocation_cinfo{};
	allocation_cinfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocation_cinfo.usage = VMA_MEMORY_USAGE_AUTO;
	return allocate_buffer(buffer_cinfo, allocation_cinfo);
}

// Helper function to invoke buffer constructor.
Buffer DeviceMemoryAllocator::allocate_buffer(vk::BufferCreateInfo &buffer_cinfo, VmaAllocationCreateInfo &allocation_cinfo) const
{
	return Buffer(Key<DeviceMemoryAllocator>{}, handle_, buffer_cinfo, allocation_cinfo);
}

// Allocate a null buffer.
Buffer DeviceMemoryAllocator::allocate_null_buffer() const
{
	return Buffer(Key<DeviceMemoryAllocator>{}, handle_, nullptr);
}

// Allocate Image that will only be used on GPU.
Image DeviceMemoryAllocator::allocate_device_only_image(vk::ImageCreateInfo &image_cinfo) const
{
	VmaAllocationCreateInfo allocation_cinfo{};
	allocation_cinfo.flags    = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocation_cinfo.usage    = VMA_MEMORY_USAGE_AUTO;
	allocation_cinfo.priority = 1.0f;
	return allocate_image(image_cinfo, allocation_cinfo);
}

// Allocate vkImage and the memory associated with it.
Image DeviceMemoryAllocator::allocate_image(vk::ImageCreateInfo &image_cinfo, VmaAllocationCreateInfo &allocation_cinfo) const
{
	return Image(Key<DeviceMemoryAllocator>{}, handle_, image_cinfo, allocation_cinfo);
}

// Allocate null image.
Image DeviceMemoryAllocator::allocate_null_image() const
{
	return Image(Key<DeviceMemoryAllocator>{}, handle_, nullptr);
};

}        // namespace W3D