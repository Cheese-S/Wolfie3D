#pragma once

#include <unordered_map>

#include "common/vk_common.hpp"

namespace W3D
{
class Device;

// Struct that contain both set layout and set.
struct DescriptorAllocation
{
	vk::DescriptorSetLayout set_layout;
	vk::DescriptorSet       set;
};

// Helper Class responsible for allocating descriptor sets.
// It manages a free list and a used list of descriptor pools.
class DescriptorAllocator
{
	struct PoolSizeFactor
	{
		vk::DescriptorType type;
		float              coeff;
	};

  public:
	const static std::vector<PoolSizeFactor> DESCRIPTOR_SIZE_FACTORS;
	const static uint32_t                    DEFAULT_SIZE;

	DescriptorAllocator(Device &device);
	~DescriptorAllocator();

	vk::DescriptorSet allocate(vk::DescriptorSetLayout &layout);
	void              reset_pools();
	const Device     &get_device();

  private:
	Device            &device_;
	vk::DescriptorPool grab_pool();
	vk::DescriptorPool create_pool();

	vk::DescriptorPool              current_pool_{nullptr};        // the pool we allocate things from
	std::vector<vk::DescriptorPool> free_pools_;                   // contain all free pools
	std::vector<vk::DescriptorPool> used_pools_;                   // contain all pools that has been used / in use (current_pool_)
};

// A cahce from descriptor set layouts.
// * This class allocates descriptor layouts and thus destroys them.
class DescriptorLayoutCache
{
  public:
	// Wrapper for bindings. We supply our own == operator and hash so that it can be used as a key in unordered_map.
	struct DescriptorSetLayoutDetails
	{
		std::vector<vk::DescriptorSetLayoutBinding> bindings;
		bool                                        operator==(const DescriptorSetLayoutDetails &other) const;
		size_t                                      hash() const;
	};

	DescriptorLayoutCache(Device &device);
	~DescriptorLayoutCache();

	vk::DescriptorSetLayout create_descriptor_layout(
	    vk::DescriptorSetLayoutCreateInfo &layout_cinfo);

  private:
	struct DescriptorLayoutHash
	{
		std::size_t operator()(const DescriptorSetLayoutDetails &k) const
		{
			return k.hash();
		}
	};
	Device &device_;
	std::unordered_map<DescriptorSetLayoutDetails, vk::DescriptorSetLayout, DescriptorLayoutHash>
	    cache_;
};

// Helper class to build descriptor set. (see Builder design pattern)
// Instead of: create a pool -> create a descriptor set layout -> allocate descriptor set -> write to the descriptor set
// DescriptorBuilder build descriptor sets by binding buffer and binding images.
// This class returns both the set layout (needed for pipeline layout) and an updated descriptor set.
class DescriptorBuilder
{
  public:
	static DescriptorBuilder begin(DescriptorLayoutCache &layout_cache,
	                               DescriptorAllocator   &descriptor_allocator);
	DescriptorBuilder       &bind_buffer(uint32_t binding, vk::DescriptorBufferInfo &buffer_info,
	                                     vk::DescriptorType type, vk::ShaderStageFlags flasg);
	DescriptorBuilder       &bind_image(uint32_t binding, vk::DescriptorImageInfo &image_info,
	                                    vk::DescriptorType type, vk::ShaderStageFlags flags);
	DescriptorAllocation     build();

  private:
	DescriptorBuilder(DescriptorLayoutCache &layout_cache,
	                  DescriptorAllocator   &descriptor_allocator);
	std::vector<vk::WriteDescriptorSet>         writes_;
	std::vector<vk::DescriptorSetLayoutBinding> layout_bindings_;

	DescriptorLayoutCache &layout_cache_;
	DescriptorAllocator   &allocator_;
};

struct DescriptorState
{
	DescriptorState(Device &device) :
	    allocator(device),
	    cache(device)
	{
	}
	DescriptorAllocator   allocator;
	DescriptorLayoutCache cache;
};
}        // namespace W3D