
#include "memory.hpp"

#include <memory>
#include <stdexcept>

#include "common.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define ERR_GUARD_VULKAN(expr)                                                                 \
    do {                                                                                       \
        if ((expr) < 0) {                                                                      \
            assert(0 && #expr);                                                                \
            throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
        }                                                                                      \
    } while (false)

namespace W3D::DeviceMemory {
Allocator::Allocator(const Instance& instance, const Device& device) {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    createInfo.physicalDevice = *instance.physicalDevice();
    createInfo.device = *device.handle();
    createInfo.instance = *instance.handle();
    vmaCreateAllocator(&createInfo, &allocator_);
}

Allocator::~Allocator() { vmaDestroyAllocator(allocator_); }

std::unique_ptr<Buffer> Allocator::allocateStagingBuffer(size_t size) {
    vk::BufferCreateInfo bufferInfo({}, size, vk::BufferUsageFlagBits::eTransferSrc);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateVertexBuffer(size_t size) {
    vk::BufferCreateInfo bufferInfo(
        {}, size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = 0;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateIndexBuffer(size_t size) {
    vk::BufferCreateInfo bufferInfo(
        {}, size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = 0;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateUniformBuffer(size_t size) {
    vk::BufferCreateInfo bufferCreateInfo(
        {}, size, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return allocateBuffer(bufferCreateInfo, allocCreateInfo);
}

std::unique_ptr<Image> Allocator::allocateAttachmentImage(vk::ImageCreateInfo& imageCreateInfo) {
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority = 1.0f;

    return allocateImage(imageCreateInfo, allocCreateInfo);
}

std::unique_ptr<Image> Allocator::allocateImage(vk::ImageCreateInfo& imageCreateInfo,
                                                VmaAllocationCreateInfo& allocCreateInfo) {
    return std::unique_ptr<Image>(new Image(allocator_, imageCreateInfo, allocCreateInfo));
}

std::unique_ptr<Buffer> Allocator::allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo,
                                                  VmaAllocationCreateInfo& allocCreateInfo) {
    return std::unique_ptr<Buffer>(new Buffer(allocator_, bufferCreateInfo, allocCreateInfo));
}

Object::Object(VmaAllocator& allocator) : allocator_(allocator){};

void Object::updateFlags() { vmaGetAllocationMemoryProperties(allocator_, allocation_, &flags_); }

Image::Image(VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo,
             VmaAllocationCreateInfo& allocCreateInfo)
    : Object(allocator) {
    ERR_GUARD_VULKAN(vmaCreateImage(allocator_,
                                    reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo),
                                    &allocCreateInfo, &image_, &allocation_, &allocationInfo_));
    updateFlags();
}

Image::~Image() { vmaDestroyImage(allocator_, image_, allocation_); }

Buffer::Buffer(VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo,
               VmaAllocationCreateInfo& allocCreateInfo)
    : Object(allocator) {
    ERR_GUARD_VULKAN(vmaCreateBuffer(allocator_,
                                     reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo),
                                     &allocCreateInfo, &buffer_, &allocation_, &allocationInfo_));
    updateFlags();
}

Buffer::~Buffer() { vmaDestroyBuffer(allocator_, buffer_, allocation_); }

}  // namespace W3D::DeviceMemory