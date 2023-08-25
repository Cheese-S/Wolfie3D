#pragma once

#include "scene_graph/component.hpp"

namespace W3D
{

class ImageResource;
class Sampler;

namespace sg
{

class Texture : public Component
{
  public:
	Texture(const std::string &name);
	Texture(Texture &&other) = default;

	virtual ~Texture() = default;
	virtual std::type_index get_type() override;

	ImageResource *p_resource_ = nullptr;
	Sampler       *p_sampler_  = nullptr;

  private:
};
}        // namespace sg

};        // namespace W3D