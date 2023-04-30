#pragma once

#include <unordered_map>

#include "common/common.hpp"

namespace W3D {
class Device;

class DescriptorAllocator {
    struct PoolSizeFactor {
        vk::DescriptorType type;
        float factor;
    };

   public:
    const static std::vector<PoolSizeFactor> DESCRIPTOR_SIZE_FACTORS;
    const static uint32_t DEFAULT_SIZE;

    DescriptorAllocator(const Device* pDevice);
    ~DescriptorAllocator();

    vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);
    void reset_pools();
    const Device& get_device();

   private:
    const Device* pDevice_;
    vk::DescriptorPool grab_pool();
    vk::DescriptorPool create_pool();

    vk::DescriptorPool current_pool{nullptr};
    std::vector<vk::DescriptorPool> used_pools_;
    std::vector<vk::DescriptorPool> free_pools_;
};

class DescriptorLayoutCache {
   public:
    struct DescriptorLayoutInfo {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bool operator==(const DescriptorLayoutInfo& other) const;
        size_t hash() const;
    };

    DescriptorLayoutCache(const Device& device);
    vk::DescriptorSetLayout create_descriptor_layout(
        vk::DescriptorSetLayoutCreateInfo* create_info);

   private:
    struct DescriptorLayoutHash {
        std::size_t operator()(const DescriptorLayoutInfo& k) const {
            return k.hash();
        }
    };
    const Device* pDevice_;
    std::unordered_map<DescriptorLayoutInfo, vk::raii::DescriptorSetLayout, DescriptorLayoutHash>
        cache_;
};

class DescriptorBuilder {
   public:
    static DescriptorBuilder begin(DescriptorLayoutCache* layout_cache,
                                   DescriptorAllocator* allocator);
    DescriptorBuilder& bind_buffer(uint32_t binding, vk::DescriptorBufferInfo* buffer_info,
                                   vk::DescriptorType type, vk::ShaderStageFlags flasg);
    DescriptorBuilder& bind_image(uint32_t binding, vk::DescriptorImageInfo* image_info,
                                  vk::DescriptorType type, vk::ShaderStageFlags flags);
    vk::DescriptorSet build(vk::DescriptorSetLayout& layout);

   private:
    std::vector<vk::WriteDescriptorSet> writes_;
    std::vector<vk::DescriptorSetLayoutBinding> bindings_;

    DescriptorLayoutCache* pCache_;
    DescriptorAllocator* pAllocator_;
};
}  // namespace W3D