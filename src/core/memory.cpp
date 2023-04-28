
#include "memory.hpp"

#include <memory>
#include <stdexcept>

#include "device.hpp"
#include "instance.hpp"

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

Allocator::~Allocator() {
    vmaDestroyAllocator(allocator_);
}

std::unique_ptr<Buffer> Allocator::allocateStagingBuffer(size_t size) const {
    vk::BufferCreateInfo bufferInfo({}, size, vk::BufferUsageFlagBits::eTransferSrc);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateVertexBuffer(size_t size) const {
    vk::BufferCreateInfo bufferInfo(
        {}, size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = 0;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateIndexBuffer(size_t size) const {
    vk::BufferCreateInfo bufferInfo(
        {}, size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = 0;
    return allocateBuffer(bufferInfo, allocCreateInfo);
}

std::unique_ptr<Buffer> Allocator::allocateUniformBuffer(size_t size) const {
    vk::BufferCreateInfo bufferCreateInfo(
        {}, size, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return allocateBuffer(bufferCreateInfo, allocCreateInfo);
}

std::unique_ptr<Image> Allocator::allocateAttachmentImage(
    vk::ImageCreateInfo& imageCreateInfo) const {
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority = 1.0f;

    return allocateImage(imageCreateInfo, allocCreateInfo);
}

std::unique_ptr<Image> Allocator::allocate_device_only_image(
    vk::ImageCreateInfo& image_info) const {
    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    alloc_create_info.priority = 1.0f;
    return allocateImage(image_info, alloc_create_info);
}

std::unique_ptr<Image> Allocator::allocateImage(vk::ImageCreateInfo& imageCreateInfo,
                                                VmaAllocationCreateInfo& allocCreateInfo) const {
    return std::make_unique<Image>(allocator_, imageCreateInfo, allocCreateInfo,
                                   Object::AllocToken{});
}

std::unique_ptr<Buffer> Allocator::allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo,
                                                  VmaAllocationCreateInfo& allocCreateInfo) const {
    return std::make_unique<Buffer>(allocator_, bufferCreateInfo, allocCreateInfo,
                                    Object::AllocToken{});
}

Object::Object(const VmaAllocator& allocator) : allocator_(allocator){};

void Object::update_flags() {
    vmaGetAllocationMemoryProperties(allocator_, allocation_, &flags_);
}

Image::Image(const VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo,
             VmaAllocationCreateInfo& allocCreateInfo, AllocToken token)
    : Object(allocator) {
    ERR_GUARD_VULKAN(vmaCreateImage(allocator_,
                                    reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo),
                                    &allocCreateInfo, &image_, &allocation_, &allocationInfo_));
    update_flags();
}

Image::~Image() {
    vmaDestroyImage(allocator_, image_, allocation_);
}

Buffer::Buffer(const VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo,
               VmaAllocationCreateInfo& allocCreateInfo, AllocToken token)
    : Object(allocator) {
    is_persistent_ = allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT;
    ERR_GUARD_VULKAN(vmaCreateBuffer(allocator_,
                                     reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo),
                                     &allocCreateInfo, &buffer_, &allocation_, &allocationInfo_));
    update_flags();
}

Buffer::~Buffer() {
    vmaDestroyBuffer(allocator_, buffer_, allocation_);
}

VkBuffer Buffer::handle() {
    return buffer_;
}

void Buffer::update(const std::vector<uint8_t>& data, size_t offset) {
    update(data.data(), data.size(), offset);
}

void Buffer::update(void* data, size_t size, size_t offset) {
    update(reinterpret_cast<const uint8_t*>(data), size, offset);
}

void Buffer::update(const uint8_t* data, size_t size, size_t offset) {
    if (is_persistent_) {
        std::copy(data, data + size, reinterpret_cast<uint8_t*>(allocationInfo_.pMappedData));
    } else {
        map();
        std::copy(data, data + size, reinterpret_cast<uint8_t*>(pMapped_data_));
        flush();
        unmap();
    }
}

void Buffer::map() {
    assert(is_mappable());
    if (!is_mapped_) {
        vmaMapMemory(allocator_, allocation_, &pMapped_data_);
    }
    is_mapped_ = true;
}

void Buffer::flush() {
    if (is_mapped_) {
        vmaFlushAllocation(allocator_, allocation_, 0, allocationInfo_.size);
    }
}

void Buffer::unmap() {
    if (is_mapped_) {
        vmaUnmapMemory(allocator_, allocation_);
        is_mapped_ = false;
    }
}

}  // namespace W3D::DeviceMemory