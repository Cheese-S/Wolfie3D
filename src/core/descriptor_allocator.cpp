#include "descriptor_allocator.hpp"

#include "common/utils.hpp"
#include "device.hpp"
#include "vulkan/vulkan_hash.hpp"

namespace W3D
{

// Mental Model: DescriptorSets contains a set of ptrs that points to resources (buffers / images).
// To allocate a DescriptorSet, we need to first specify its layout (what images/buffers are binded to which slot)

/* ---------------------------- DescriptorBuilder --------------------------- */

// Constructor for builder. This is only called by the begin method.
DescriptorBuilder::DescriptorBuilder(DescriptorLayoutCache &layout_cache, DescriptorAllocator &allocator) :
    layout_cache_(layout_cache),
    allocator_(allocator)
{
}

// Helper function to begin a building process.
DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache &layout_cache,
                                           DescriptorAllocator   &allocator)
{
	DescriptorBuilder builder(layout_cache, allocator);
	return builder;
}

// bind the buffer to specified slot.
// * Binding to the same slot will cause earlier binding to be overwritten.
DescriptorBuilder &DescriptorBuilder::bind_buffer(uint32_t                  binding,
                                                  vk::DescriptorBufferInfo &buffer_info,
                                                  vk::DescriptorType        type,
                                                  vk::ShaderStageFlags      flags)
{
	vk::DescriptorSetLayoutBinding new_layout_binding;
	new_layout_binding.descriptorCount = 1;
	new_layout_binding.descriptorType  = type;
	new_layout_binding.stageFlags      = flags;
	new_layout_binding.binding         = binding;

	layout_bindings_.push_back(new_layout_binding);

	vk::WriteDescriptorSet write;
	write.descriptorCount = 1;
	write.descriptorType  = type;
	write.pBufferInfo     = &buffer_info;
	write.dstBinding      = binding;

	writes_.push_back(write);
	return *this;
}

// bind the image to the specified slot.
// * Binding to the same slot will cause earlier binding to be overwritten.
DescriptorBuilder &DescriptorBuilder::bind_image(uint32_t                 binding,
                                                 vk::DescriptorImageInfo &image_info,
                                                 vk::DescriptorType       type,
                                                 vk::ShaderStageFlags     flags)
{
	vk::DescriptorSetLayoutBinding new_layout_binding;
	new_layout_binding.descriptorCount = 1;
	new_layout_binding.descriptorType  = type;
	new_layout_binding.stageFlags      = flags;
	new_layout_binding.binding         = binding;
	layout_bindings_.push_back(new_layout_binding);

	vk::WriteDescriptorSet write;
	write.descriptorCount = 1;
	write.descriptorType  = type;
	write.pImageInfo      = &image_info;
	write.dstBinding      = binding;
	writes_.push_back(write);
	return *this;
}

// Actually allocating the set and write to that set.
// Retrieve a set layout with the specified layout_cinfo from layout cache.
DescriptorAllocation DescriptorBuilder::build()
{
	vk::DescriptorSetLayoutCreateInfo layout_cinfo{
	    .bindingCount = to_u32(layout_bindings_.size()),
	    .pBindings    = layout_bindings_.data(),
	};
	vk::DescriptorSetLayout set_layout = layout_cache_.create_descriptor_layout(layout_cinfo);
	vk::DescriptorSet       set        = allocator_.allocate(set_layout);
	for (auto &write : writes_)
	{
		write.dstSet = set;
	}
	allocator_.get_device().get_handle().updateDescriptorSets(writes_, {});
	return {
	    .set_layout = set_layout,
	    .set        = set,
	};
}

/* -------------------------- DESCRIPTOR ALLOCATOR -------------------------- */

// Sensible defaults for different types of descriptors.
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

DescriptorAllocator::DescriptorAllocator(Device &device) :
    device_(device){};

DescriptorAllocator::~DescriptorAllocator()
{
	for (auto p : free_pools_)
	{
		device_.get_handle().destroyDescriptorPool(p);
	}
	for (auto p : used_pools_)
	{
		device_.get_handle().destroyDescriptorPool(p);
	}
}

// Allocate descriptor set with the given layout from a free pool.
vk::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout &layout)
{
	if (!current_pool_)
	{
		current_pool_ = grab_pool();
		used_pools_.push_back(current_pool_);
	}
	vk::DescriptorSetAllocateInfo descriptor_set_ainfo{
	    .descriptorPool     = current_pool_,
	    .descriptorSetCount = 1,
	    .pSetLayouts        = &layout,
	};

	try
	{
		auto descriptor_set = device_.get_handle().allocateDescriptorSets(descriptor_set_ainfo);
		return descriptor_set[0];
	}
	catch (vk::FragmentedPoolError &err)
	{
	}
	catch (vk::OutOfPoolMemoryError &err)
	{
	}
	catch (...)
	{
		// Rarely, there is some error that is not pool realted. We cannot handle it. Return a null one.
		return vk::DescriptorSet{nullptr};
	}

	// The old current_pool_ has run out of some descriptors.
	current_pool_ = grab_pool();
	used_pools_.push_back(current_pool_);
	descriptor_set_ainfo.descriptorPool = current_pool_;
	return device_.get_handle().allocateDescriptorSets(descriptor_set_ainfo)[0];
}

// Return a free pool.
vk::DescriptorPool DescriptorAllocator::grab_pool()
{
	if (free_pools_.size() > 0)
	{
		vk::DescriptorPool pool = free_pools_.back();
		free_pools_.pop_back();
		return pool;
	}
	else
	{
		return create_pool();
	}
}

// Create a pool with specified default size.
vk::DescriptorPool DescriptorAllocator::create_pool()
{
	std::vector<vk::DescriptorPoolSize> pool_sizes;
	pool_sizes.reserve(DESCRIPTOR_SIZE_FACTORS.size());
	for (auto &factor : DESCRIPTOR_SIZE_FACTORS)
	{
		pool_sizes.emplace_back(vk::DescriptorPoolSize{factor.type, to_u32(factor.coeff * DEFAULT_SIZE)});
	}
	vk::DescriptorPoolCreateInfo pool_cinfo{};
	pool_cinfo.maxSets       = DEFAULT_SIZE;
	pool_cinfo.poolSizeCount = to_u32(pool_sizes.size());
	pool_cinfo.pPoolSizes    = pool_sizes.data();
	return device_.get_handle().createDescriptorPool(pool_cinfo);
}

// Reset all the used pools.
// ! This free all descriptors allocated from these pools.
void DescriptorAllocator::reset_pools()
{
	for (auto p : used_pools_)
	{
		device_.get_handle().resetDescriptorPool(p);
	}

	free_pools_ = used_pools_;
	used_pools_.clear();
	current_pool_ = VK_NULL_HANDLE;
}

const Device &DescriptorAllocator::get_device()
{
	return device_;
}

/* -------------------------- DescriptorLayoutCache ------------------------- */

// Constructor for layout cache.
DescriptorLayoutCache::DescriptorLayoutCache(Device &device) :
    device_(device)
{
}

// Destroy all set layouts.
DescriptorLayoutCache::~DescriptorLayoutCache()
{
	auto it = cache_.begin();
	while (it != cache_.end())
	{
		device_.get_handle().destroyDescriptorSetLayout(it->second);
		it++;
	}
}

// create the set layout with the given create info.
vk::DescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(
    vk::DescriptorSetLayoutCreateInfo &layout_cinfo)
{
	DescriptorSetLayoutDetails layout_details{};
	layout_details.bindings.reserve(layout_cinfo.bindingCount);
	bool    is_sorted    = true;
	int32_t last_binding = -1;

	for (uint32_t i = 0; i < layout_cinfo.bindingCount; i++)
	{
		layout_details.bindings.push_back(layout_cinfo.pBindings[i]);

		if (to_u32(layout_cinfo.pBindings[i].binding) > last_binding)
		{
			last_binding = layout_cinfo.pBindings[i].binding;
		}
		else
		{
			is_sorted = false;
		}
	}

	vk::DescriptorSetLayoutBinding binding;

	// DescriptorBuilder does not require the bindings to be sorted.
	// We want them to be sorted to implement the cache look up logic.
	if (!is_sorted)
	{
		std::sort(layout_details.bindings.begin(), layout_details.bindings.end(), [](vk::DescriptorSetLayoutBinding &a, vk::DescriptorSetLayoutBinding &b) {
			return a.binding < b.binding;
		});
	}

	auto it = cache_.find(layout_details);
	if (it != cache_.end())
	{
		return it->second;
	}
	else
	{
		// create new set layout if there isn't one.
		cache_.insert(std::make_pair(layout_details, device_.get_handle().createDescriptorSetLayout(layout_cinfo)));
		return cache_.at(layout_details);
	}
}

// Campre LayoutDetails by comparing their bindings one by one.
bool DescriptorLayoutCache::DescriptorSetLayoutDetails::operator==(
    const DescriptorSetLayoutDetails &other) const
{
	if (other.bindings.size() != bindings.size())
	{
		return false;
	}

	for (int i = 0; i < bindings.size(); i++)
	{
		const vk::DescriptorSetLayoutBinding &other_binding = other.bindings[i];
		const vk::DescriptorSetLayoutBinding &self_binding  = bindings[i];
		if (other_binding.binding != self_binding.binding ||
		    other_binding.descriptorType != self_binding.descriptorType ||
		    other_binding.descriptorCount != self_binding.descriptorCount ||
		    other_binding.stageFlags != self_binding.stageFlags)
		{
			return false;
		}
	}
	return true;
}

// Hash function for LayoutDetails. We uses vulkan.hpp's hash function for vk::DescriptorSetLayoutBinding
size_t DescriptorLayoutCache::DescriptorSetLayoutDetails::hash() const
{
	using std::hash;
	using std::size_t;

	size_t result = hash<size_t>()(bindings.size());
	for (const auto &b : bindings)
	{
		result ^= hash<vk::DescriptorSetLayoutBinding>()(b);
	}

	return result;
}

}        // namespace W3D