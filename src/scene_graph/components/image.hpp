#pragma once

#include <stdint.h>

#include "common/vk_common.hpp"
#include "core/image_resource.hpp"
#include "scene_graph/component.hpp"

namespace W3D
{

class Device;

namespace sg
{
// Image Component. Wraps around an image resource.
class Image : public Component
{
  public:
	Image(ImageResource &&resrc, const std::string &name);
	Image(Image &&);

	virtual ~Image() = default;
	virtual std::type_index get_type() override;

	ImageResource &get_resource();

	void set_resource(ImageResource &&resource);

  private:
	ImageResource resource_;
};
}        // namespace sg

}        // namespace W3D