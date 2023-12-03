#pragma once

#include "common/vk_common.hpp"
#include "core/vulkan_object.hpp"

namespace W3D
{
class Device;
class PhysicalDevice;

// Wrapper class for VkSampler
// Sampler is responsible for describing how a texture should be sampled.
class Sampler : public VulkanObject<vk::Sampler>
{
  public:
	static vk::SamplerCreateInfo linear_clamp_cinfo(const PhysicalDevice &physical_device, float max_lod);

	Sampler(const Device &device, std::nullptr_t nptr);
	Sampler(const Device &device, vk::SamplerCreateInfo &sampler_cinfo);
	Sampler(Sampler &&rhs);
	~Sampler() override;

  private:
	const Device &device_;
};
}        // namespace W3D