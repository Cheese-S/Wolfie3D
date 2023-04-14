#pragma once

#include <memory>

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1002000

// Macros used to disable irrelevant warnings
// see https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/issues/283
#pragma clang dianostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#include <vk_mem_alloc.h>
#pragma clang dianostic pop

#include "common.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_structs.hpp"

namespace W3D::DeviceMemory {

class Object {
    friend class Allocator;

   public:
    virtual ~Object() = default;
    Object() = delete;
    Object(Object const&) = delete;
    void operator=(Object const&) = delete;

   protected:
    Object(VmaAllocator& allocator);
    void updateFlags();

    VmaAllocator allocator_;
    VmaAllocation allocation_;
    VmaAllocationInfo allocationInfo_;

   private:
    VkMemoryPropertyFlags flags_;
};

class Buffer : Object {
    friend class Allocator;

   public:
    ~Buffer();
    Buffer() = delete;
    Buffer(Buffer const&) = delete;
    void operator=(Buffer const&) = delete;

   private:
    Buffer(VmaAllocator& allocator, vk::BufferCreateInfo& bufferCreateInfo);
    VkBuffer buffer_;
};

class Image : Object {
    friend class Allocator;

   public:
    ~Image();
    Image() = delete;
    Image(Image const&) = delete;
    void operator=(Image const&) = delete;

   private:
    Image(VmaAllocator& allocator, vk::ImageCreateInfo& imageCreateInfo);
    VkImage image_;
};

class Allocator {
    friend class Object;

   public:
    Allocator(const Instance& instance, const Device& device);
    ~Allocator();
    Allocator() = delete;
    Allocator(Allocator const&) = delete;
    void operator=(Allocator const&) = delete;

    std::unique_ptr<Buffer> allocateBuffer(vk::BufferCreateInfo& bufferCreateInfo);
    std::unique_ptr<Image> allocateImage(vk::ImageCreateInfo& imageCreateInfo);

   private:
    VmaAllocator allocator_;
};

}  // namespace W3D::DeviceMemory
