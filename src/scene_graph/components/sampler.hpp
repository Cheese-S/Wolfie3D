#pragma once

#include "common/vk_common.hpp"
#include "core/sampler.hpp"
#include "scene_graph/component.hpp"

namespace W3D
{
class Device;

namespace sg
{
// Component wrapper for sampler.
class Sampler : public Component
{
  public:
	Sampler(const Device &device, const std::string &name, vk::SamplerCreateInfo &sampler_cinfo);

	virtual ~Sampler() override = default;
	virtual std::type_index get_type() override;
	vk::Sampler             get_handle();

  private:
	const Device &device_;
	W3D::Sampler  sampler_;
};
}        // namespace sg
}        // namespace W3D