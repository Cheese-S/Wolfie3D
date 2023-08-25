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
class Image : public Component
{
  public:
	Image(ImageLoadResult &&result, const std::string &name);
	Image(ImageResource &&resource, ImageTransferInfo &&image_tinfo, const std::string &name);
	Image(Image &&);

	virtual ~Image() = default;
	virtual std::type_index get_type() override;

	ImageResource           &get_resource();
	const ImageTransferInfo &get_image_transferinfo();

  private:
	ImageResource                      resource_;
	std::unique_ptr<ImageTransferInfo> p_image_info_ = nullptr;
};
}        // namespace sg

}        // namespace W3D