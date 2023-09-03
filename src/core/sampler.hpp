#pragma once

#include "common/vk_common.hpp"
#include "core/vulkan_object.hpp"

namespace W3D
{
class Device;
class PhysicalDevice;

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