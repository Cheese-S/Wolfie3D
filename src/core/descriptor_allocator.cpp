#include "descriptor_allocator.hpp"

#include "device.hpp"
#include "vulkan/vulkan_hash.hpp"

namespace W3D {

const std::vector<DescriptorAllocator::PoolSizeFactor>
    DescriptorAllocator::DESCRIPTOR_SIZE_FACTORS = {
        {vk::DescriptorType::eSampler, 0.5f},
        {vk::DescriptorType::eCombinedImageSampler, 4.0f},
        {vk::DescriptorType::eSampledImage, 4.0f},
        {vk::DescriptorType::eStorageImage, 1.0f},
        {vk::DescriptorType::eUniformTexelBuffer, 1.0f},
        {vk::DescriptorType::eStorageTexelBuffer, 1.0f},
        {vk::DescriptorType::eUniformBuffer, 2.0f},
        {vk::DescriptorType::eStorageBuffer, 2.0f},
        {vk::DescriptorType::eUniformBufferDynamic, 1.0f},
        {vk::DescriptorType::eStorageBufferDynamic, 1.0f},
        {vk::DescriptorType::eInputAttachment, 0.5f}};

const uint32_t DescriptorAllocator::DEFAULT_SIZE = 1000;

DescriptorAllocator::DescriptorAllocator(const Device* pDevice) {
    pDevice_ = pDevice;
};

DescriptorAllocator::~DescriptorAllocator() {
    for (auto p : free_pools_) {
        (*pDevice_->handle()).destroyDescriptorPool(p);
    }
    for (auto p : used_pools_) {
        (*pDevice_->handle()).destroyDescriptorPool(p);
    }
}

vk::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
    if (!current_pool) {
        current_pool = grab_pool();
        used_pools_.push_back(current_pool);
    }

    vk::DescriptorSetAllocateInfo alloc_info(current_pool, 1, &layout);

    try {
        auto descriptor_set = (*pDevice_->handle()).allocateDescriptorSets(alloc_info);
        return descriptor_set[0];
    } catch (vk::FragmentedPoolError& err) {
    } catch (vk::OutOfPoolMemoryError& err) {
    } catch (...) {
        return vk::DescriptorSet{nullptr};
    }

    current_pool = grab_pool();
    used_pools_.push_back(current_pool);
    alloc_info.descriptorPool = current_pool;
    return (*pDevice_->handle()).allocateDescriptorSets(alloc_info)[0];
}

vk::DescriptorPool DescriptorAllocator::grab_pool() {
    if (free_pools_.size() > 0) {
        vk::DescriptorPool pool = free_pools_.back();
        free_pools_.pop_back();
        return pool;
    } else {
        return create_pool();
    }
}

vk::DescriptorPool DescriptorAllocator::create_pool() {
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(DESCRIPTOR_SIZE_FACTORS.size());
    for (auto& factor : DESCRIPTOR_SIZE_FACTORS) {
        pool_sizes.push_back({factor.type, static_cast<uint32_t>(factor.factor * DEFAULT_SIZE)});
    }
    vk::DescriptorPoolCreateInfo pool_info({}, DEFAULT_SIZE, pool_sizes);
    return (*pDevice_->handle()).createDescriptorPool(pool_info);
}

void DescriptorAllocator::reset_pools() {
    for (auto p : used_pools_) {
        (*pDevice_->handle()).resetDescriptorPool(p);
    }

    free_pools_ = used_pools_;
    used_pools_.clear();
    current_pool = VK_NULL_HANDLE;
}

const Device& DescriptorAllocator::get_device() {
    return *pDevice_;
}

DescriptorLayoutCache::DescriptorLayoutCache(const Device& device) {
    pDevice_ = &device;
}

vk::DescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(
    vk::DescriptorSetLayoutCreateInfo* create_info) {
    DescriptorLayoutInfo layout_info;
    layout_info.bindings.reserve(create_info->bindingCount);
    bool is_sorted = true;
    int32_t last_binding = -1;

    for (uint32_t i = 0; i < create_info->bindingCount; i++) {
        layout_info.bindings.push_back(create_info->pBindings[i]);

        if (static_cast<int32_t>(create_info->pBindings[i].binding) > last_binding) {
            last_binding = create_info->pBindings[i].binding;
        } else {
            is_sorted = false;
        }
    }

    if (!is_sorted) {
        std::sort(layout_info.bindings.begin(), layout_info.bindings.end(),
                  [](vk::DescriptorSetLayoutBinding& a, vk::DescriptorSetLayoutBinding& b) {
                      return a.binding < b.binding;
                  });
    }

    auto it = cache_.find(layout_info);
    if (it != cache_.end()) {
        return *(*it).second;
    } else {
        cache_.insert(std::make_pair(layout_info,
                                     pDevice_->handle().createDescriptorSetLayout(*create_info)));
        return *cache_.at(layout_info);
    }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    const DescriptorLayoutInfo& other) const {
    if (other.bindings.size() != bindings.size()) {
        return false;
    }

    for (int i = 0; i < bindings.size(); i++) {
        const auto& other_binding = other.bindings[i];
        const auto& self_binding = bindings[i];
        if (other_binding.binding != self_binding.binding ||
            other_binding.descriptorType != self_binding.descriptorType ||
            other_binding.descriptorCount != self_binding.descriptorCount ||
            other_binding.stageFlags != self_binding.stageFlags) {
            return false;
        }
    }
    return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
    using std::hash;
    using std::size_t;

    size_t result = hash<size_t>()(bindings.size());
    for (const auto& b : bindings) {
        result ^= hash<vk::DescriptorSetLayoutBinding>()(b);
    }

    return result;
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layout_cache,
                                           DescriptorAllocator* allocator) {
    DescriptorBuilder builder;
    builder.pCache_ = layout_cache;
    builder.pAllocator_ = allocator;
    return builder;
}

DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding,
                                                  vk::DescriptorBufferInfo* buffer_info,
                                                  vk::DescriptorType type,
                                                  vk::ShaderStageFlags flags) {
    vk::DescriptorSetLayoutBinding new_binding;
    new_binding.descriptorCount = 1;
    new_binding.descriptorType = type;
    new_binding.stageFlags = flags;
    new_binding.binding = binding;

    bindings_.push_back(new_binding);

    vk::WriteDescriptorSet write;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = buffer_info;
    write.dstBinding = binding;

    writes_.push_back(write);
    return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding,
                                                 vk::DescriptorImageInfo* image_info,
                                                 vk::DescriptorType type,
                                                 vk::ShaderStageFlags flags) {
    vk::DescriptorSetLayoutBinding new_binding;
    new_binding.descriptorCount = 1;
    new_binding.descriptorType = type;
    new_binding.stageFlags = flags;
    new_binding.binding = binding;

    bindings_.push_back(new_binding);

    vk::WriteDescriptorSet write;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = image_info;
    write.dstBinding = binding;

    writes_.push_back(write);
    return *this;
}

vk::DescriptorSet DescriptorBuilder::build(vk::DescriptorSetLayout& layout) {
    vk::DescriptorSetLayoutCreateInfo layout_info({}, bindings_);

    layout = pCache_->create_descriptor_layout(&layout_info);
    auto set = pAllocator_->allocate(layout);
    if (!set) {
        return set;
    }
    for (auto& write : writes_) {
        write.dstSet = set;
    }

    pAllocator_->get_device().handle().updateDescriptorSets(writes_, {});
    return set;
}

}  // namespace W3D