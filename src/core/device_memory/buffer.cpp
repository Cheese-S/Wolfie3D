#include "buffer.hpp"
#include "common/error.hpp"
#include "common/utils.hpp"

namespace W3D
{

// Allocate a null buffer.
Buffer::Buffer(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, std::nullptr_t nptr) :
    DeviceMemoryObject(allocator, key)
{
	handle_ = nullptr;
}

// Allocate a buffer and memory.
Buffer::Buffer(Key<DeviceMemoryAllocator> key, VmaAllocator allocator, vk::BufferCreateInfo &buffer_cinfo, VmaAllocationCreateInfo &allocation_cinfo) :
    DeviceMemoryObject(allocator, key)
{
	details_.allocator = allocator;
	is_persistent_     = allocation_cinfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT;
	VkBuffer c_buf_handle;
	VK_CHECK(vmaCreateBuffer(details_.allocator, reinterpret_cast<VkBufferCreateInfo *>(&buffer_cinfo), &allocation_cinfo, &c_buf_handle, &details_.allocation, &details_.allocation_info));
	handle_ = c_buf_handle;
	update_flags();
}

// Destroy the buffer and free its memory if not null.
Buffer::~Buffer()
{
	if (handle_)
	{
		vmaDestroyBuffer(details_.allocator, handle_, details_.allocation);
	}
};

Buffer::Buffer(Buffer &&rhs) :
    DeviceMemoryObject(std::move(rhs)),
    is_persistent_(rhs.is_persistent_),
    p_mapped_data_(rhs.p_mapped_data_)
{
	rhs.p_mapped_data_ = nullptr;
}

// Overloaded helper function that update the buffer with the given data.
// * p_data can points to any type. But, everything is handled as byte pointers.
void Buffer::update(void *p_data, size_t size, size_t offset)
{
	update(to_ubyte_ptr(p_data), size, offset);
}

// Overloaded Helper function that update the buffer.
void Buffer::update(const std::vector<uint8_t> &binary, size_t offset)
{
	update(binary.data(), binary.size(), offset);
}

// Helper function that updates the buffer.
// ! The size and offset should be in terms of bytes.
// ! If the offset + size exceeds the buffer size, it results in UB.
void Buffer::update(const uint8_t *p_data, size_t size, size_t offset)
{
	// If the buffer is always mapped in memory, simply write to that address.
	if (is_persistent_)
	{
		std::copy(p_data, p_data + size, to_ubyte_ptr(details_.allocation_info.pMappedData));
	}
	else
	{
		// We need to map the buffer first and then write to it.
		map();
		std::copy(p_data, p_data + size, to_ubyte_ptr(p_mapped_data_));
		flush();
		unmap();
	}
}

// Map the buffer if mappable.
void Buffer::map()
{
	assert(is_mappable());
	if (p_mapped_data_)
	{
		vmaMapMemory(details_.allocator, details_.allocation, &p_mapped_data_);
	}
}

// Unmap the buffer if previously mapped.
void Buffer::unmap()
{
	if (p_mapped_data_)
	{
		vmaUnmapMemory(details_.allocator, details_.allocation);
		p_mapped_data_ = nullptr;
	}
}

// Write the mapped data to GPU
void Buffer::flush()
{
	if (p_mapped_data_)
	{
		vmaFlushAllocation(details_.allocator, details_.allocation, 0, details_.allocation_info.size);
	}
}
}        // namespace W3D