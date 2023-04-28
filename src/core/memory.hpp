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

#include "common/common.hpp"

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
    ~Allocator();
    Allocator(const Instance& instance, const Device& device);
    Allocator() = delete;
    Allocator(Allocator const&) = delete;
    void operator=(Allocator const&) = delete;

    std::unique_ptr<Buffer> allocateStagingBuffer(size_t size) const;
    std::unique_ptr<Buffer> allocateVertexBuffer(size_t size) const;
    std::unique_ptr<Buffer> allocateIndexBuffer(size_t size) const;
    std::unique_ptr<Buffer> allocateUniformBuffer(size_t size) const;
    std::unique_ptr<Buffer> allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo,
                                           VmaAllocationCreateInfo& allocCreateInfo) const;

    std::unique_ptr<Image> allocateAttachmentImage(vk::ImageCreateInfo& imageCreateInfo) const;
    std::unique_ptr<Image> allocate_device_only_image(vk::ImageCreateInfo& image_info) const;
    std::unique_ptr<Image> allocateImage(vk::ImageCreateInfo& imageCreateInfo,
                                         VmaAllocationCreateInfo& allocCreateInfo) const;

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

    inline bool is_mappable() {
        return flags_ & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

   protected:
    struct AllocToken {};
    Object(const VmaAllocator& allocator);
    void update_flags();

    VmaAllocator allocator_;
    VmaAllocation allocation_;
    VmaAllocationInfo allocationInfo_;

   private:
    VkMemoryPropertyFlags flags_;
};

class Buffer : public Object {
    friend class Allocator;

   public:
    Buffer(const VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo,
           VmaAllocationCreateInfo& allocationInfo, AllocToken token);
    ~Buffer();
    Buffer() = delete;
    Buffer(Buffer const&) = delete;
    void operator=(Buffer const&) = delete;
    VkBuffer handle();
    void update(const std::vector<uint8_t>& data, size_t offset = 0);
    void update(void* data, size_t size, size_t offset = 0);
    void update(const uint8_t* data, size_t size, size_t offset = 0);

   private:
    void map();
    void flush();
    void unmap();
    bool is_mapped_;
    bool is_persistent_;
    void* pMapped_data_;
    VkBuffer buffer_;
};

class Image : public Object {
    friend class Allocator;

   public:
    Image(const VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo,
          VmaAllocationCreateInfo& allocationInfo, AllocToken token);
    ~Image();
    Image() = delete;
    Image(Image const&) = delete;
    void operator=(Image const&) = delete;

    inline VkImage handle() const {
        return image_;
    }

   private:
    VkImage image_;
};

}  // namespace W3D::DeviceMemory
