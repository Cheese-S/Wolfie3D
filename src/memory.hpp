#pragma once

#include <memory>

#include "vulkan/vulkan_core.h"

// Macros used to disable irrelevant warnings
// see https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/issues/283
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#include <vk_mem_alloc.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "common.hpp"

namespace W3D {
class Device;
class Instance;
}  // namespace W3D

namespace W3D::DeviceMemory {

class Buffer;
class Image;

class Allocator {
    friend class Object;

   public:
    Allocator(const Instance& instance, const Device& device);
    ~Allocator();
    Allocator() = delete;
    Allocator(Allocator const&) = delete;
    void operator=(Allocator const&) = delete;

    std::unique_ptr<Buffer> allocateStagingBuffer(size_t size);
    std::unique_ptr<Buffer> allocateVertexBuffer(size_t size);
    std::unique_ptr<Buffer> allocateIndexBuffer(size_t size);
    std::unique_ptr<Buffer> allocateUniformBuffer(size_t size);
    std::unique_ptr<Buffer> allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo,
                                           VmaAllocationCreateInfo& allocCreateInfo);

    std::unique_ptr<Image> allocateAttachmentImage(vk::ImageCreateInfo& imageCreateInfo);
    std::unique_ptr<Image> allocateImage(vk::ImageCreateInfo& imageCreateInfo,
                                         VmaAllocationCreateInfo& allocCreateInfo);

   private:
    VmaAllocator allocator_;
};

class Object {
    friend class Allocator;

   public:
    virtual ~Object() = default;
    Object() = delete;
    Object(Object const&) = delete;
    void operator=(Object const&) = delete;

    inline bool isHostVisible() { return flags_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; }

   protected:
    Object(VmaAllocator& allocator);
    void updateFlags();

    VmaAllocator allocator_;
    VmaAllocation allocation_;
    VmaAllocationInfo allocationInfo_;

   private:
    VkMemoryPropertyFlags flags_;
};

class Buffer : public Object {
    friend class Allocator;

   public:
    ~Buffer();
    Buffer() = delete;
    Buffer(Buffer const&) = delete;
    void operator=(Buffer const&) = delete;

    inline void* mappedData() { return allocationInfo_.pMappedData; }
    inline void flush() { vmaFlushAllocation(allocator_, allocation_, 0, VK_WHOLE_SIZE); }
    inline VkBuffer handle() { return buffer_; }

   private:
    Buffer(VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo,
           VmaAllocationCreateInfo& allocationInfo);
    VkBuffer buffer_;
};

class Image : public Object {
    friend class Allocator;

   public:
    ~Image();
    Image() = delete;
    Image(Image const&) = delete;
    void operator=(Image const&) = delete;

    inline VkImage handle() { return image_; }

   private:
    Image(VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo,
          VmaAllocationCreateInfo& allocationInfo);
    VkImage image_;
};

}  // namespace W3D::DeviceMemory
