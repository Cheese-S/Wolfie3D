#include "memory.hpp"

#include <memory>

#include "instance.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"

namespace W3D::DeviceMemory {
Allocator::Allocator(const Instance& instance, const Device& device) {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    createInfo.physicalDevice = *instance.physicalDevice();
    createInfo.device = *device.handle();
    createInfo.instance = *instance.instance();
    vmaCreateAllocator(&createInfo, &allocator_);
}

Allocator::~Allocator() { vmaDestroyAllocator(allocator_); }

std::unique_ptr<Image> Allocator::allocateImage(vk::ImageCreateInfo& imageCreateInfo) {
    return std::unique_ptr<Image>(new Image(allocator_, imageCreateInfo));
}

std::unique_ptr<Buffer> Allocator::allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo) {
    return std::unique_ptr<Buffer>(new Buffer(allocator_, bufferCreateInfo));
}

Object::Object(VmaAllocator& allocator) : allocator_(allocator){};

Image::Image(VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo) : Object(allocator) {
    VmaAllocationCreateInfo createInfo = {};
    createInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateImage(allocator_, reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo), &createInfo,
                   &image_, &allocation_, &allocationInfo_);
}

Image::~Image() { vmaDestroyImage(allocator_, image_, allocation_); }

Buffer::Buffer(VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo)
    : Object(allocator) {
    VmaAllocationCreateInfo createInfo = {};
    createInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateBuffer(allocator_, reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo),
                    &createInfo, &buffer_, &allocation_, &allocationInfo_);
}

Buffer::~Buffer() { vmaDestroyBuffer(allocator_, buffer_, allocation_); }

}  // namespace W3D::DeviceMemory