#pragma once

#include "common/vk_common.hpp"

#include "device_memory/image.hpp"

namespace W3D
{

struct ImageLoadResult;

struct ImageTransferInfo
{
	std::vector<uint8_t> binary;
	vk::Extent3D         extent;
	vk::Format           format;
	uint32_t             levels;
};

ImageTransferInfo stb_load(const std::string &path);
ImageTransferInfo gli_load(const std::string &path);

class ImageView;

class ImageResource
{
  public:
	static ImageLoadResult load_two_dim_image(const Device &device, const std::string &path);
	static ImageLoadResult load_cubic_image(const Device &device, const std::string &path);

	ImageResource(Image &&image, vk::ImageViewCreateInfo &view_cinfo);
	ImageResource(Image &&image);
	ImageResource(ImageResource &&rhs);
	~ImageResource();
	void create_view(const Device &device, vk::ImageViewCreateInfo &image_view_cinfo);

	Image     &get_image();
	ImageView &get_view() const;

  private:
	Image                      image_;
	std::unique_ptr<ImageView> p_view_;
};

struct ImageLoadResult
{
	ImageResource     resource;
	ImageTransferInfo image_tifno;
};
};        // namespace W3D