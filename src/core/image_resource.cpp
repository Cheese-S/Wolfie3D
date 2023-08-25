#include "image_resource.hpp"

#include <gli/gli.hpp>
#include <stb_image.h>

#include "common/file_utils.hpp"
#include "common/logging.hpp"
#include "core/device.hpp"
#include "core/image_view.hpp"

namespace W3D
{

ImageLoadResult ImageResource::load_two_dim_image(const Device &device, const std::string &path)
{
	ImageTransferInfo img_tinfo = stb_load(path);

	vk::ImageCreateInfo img_cinfo{
	    .imageType   = vk::ImageType::e2D,
	    .format      = img_tinfo.format,
	    .extent      = img_tinfo.extent,
	    .mipLevels   = img_tinfo.levels,
	    .arrayLayers = 1,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	    .sharingMode = vk::SharingMode::eExclusive,
	};

	ImageResource resource = ImageResource(device.get_device_memory_allocator().allocate_device_only_image(img_cinfo));

	vk::ImageViewCreateInfo view_cinfo = ImageView::two_dim_view_cinfo(resource.get_image().get_handle(), img_cinfo.format, vk::ImageAspectFlagBits::eColor, img_tinfo.levels);
	resource.create_view(device, view_cinfo);

	return {
	    .resource    = std::move(resource),
	    .image_tifno = std::move(img_tinfo),
	};
};

ImageTransferInfo stb_load(const std::string &path)
{
	std::string extension = fu::get_file_extension(path);
	if (extension != "jpg" || extension != "png")
	{
		LOGE("Unsupported file type! W3D only supports loading jpg/png 2d images");
		abort();
	}

	std::vector<uint8_t> raw_binary = fu::read_binary(path);
	int                  size       = raw_binary.size();
	int                  width, height;
	int                  channels;
	int                  req_channels = 4;

	stbi_uc *p_img_data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(raw_binary.data()), size, &width, &height, &channels, req_channels);

	if (!p_img_data)
	{
		LOGE("Failure to load convert raw binary to image binary: {}", stbi_failure_reason());
	}

	std::vector<uint8_t> img_binary = {p_img_data, p_img_data + size};

	stbi_image_free(p_img_data);

	return {
	    .binary = std::move(img_binary),
	    .extent = {
	        .width  = to_u32(width),
	        .height = to_u32(height),
	        .depth  = 1,
	    },
	    .format = vk::Format::eR8G8B8A8Srgb,
	    .levels = 1,
	};
}

ImageLoadResult ImageResource::load_cubic_image(const Device &device, const std::string &path)
{
	ImageTransferInfo   img_tinfo = gli_load(path);
	vk::ImageCreateInfo img_cinfo{
	    .flags       = vk::ImageCreateFlagBits::eCubeCompatible,
	    .imageType   = vk::ImageType::e2D,
	    .format      = img_tinfo.format,
	    .extent      = img_tinfo.extent,
	    .mipLevels   = img_tinfo.levels,
	    .arrayLayers = 6,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	    .sharingMode = vk::SharingMode::eExclusive,
	};

	ImageResource resource = ImageResource(device.get_device_memory_allocator().allocate_device_only_image(img_cinfo));

	vk::ImageViewCreateInfo view_cinfo = ImageView::cube_view_cinfo(resource.get_image().get_handle(), img_cinfo.format, vk::ImageAspectFlagBits::eColor, img_tinfo.levels);
	resource.create_view(device, view_cinfo);

	return {
	    .resource    = std::move(resource),
	    .image_tifno = std::move(img_tinfo),
	};
}

ImageTransferInfo gli_load(const std::string &path)
{
	static const std::unordered_map<gli::format, vk::Format> gli_to_vk_format_map = {
	    {gli::FORMAT_RGBA8_UNORM_PACK8, vk::Format::eR8G8B8A8Unorm},
	    {gli::FORMAT_RGBA32_SFLOAT_PACK32, vk::Format::eR32G32B32A32Sfloat},
	    {gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16, vk::Format::eBc3UnormBlock},
	    {gli::FORMAT_RG32_SFLOAT_PACK32, vk::Format::eR32G32Sfloat},
	    {gli::FORMAT_RGB8_UNORM_PACK8, vk::Format::eR8G8B8Unorm},
	};

	std::string extension = fu::get_file_extension(path);
	if (extension != "dds")
	{
		LOGE("Unsupported file type! W3D only supports loading .dds cubic images");
		abort();
	}

	gli::texture_cube gli_cube(gli::load(path));

	if (gli_cube.empty())
	{
		LOGE("Failed to load cubemap");
		abort();
	}

	std::vector<std::uint8_t> img_binary{gli_cube.data<uint8_t>(), gli_cube.data<uint8_t>() + gli_cube.size()};

	return {
	    .binary = std::move(img_binary),
	    .extent = {
	        .width  = to_u32(gli_cube.extent().x),
	        .height = to_u32(gli_cube.extent().y),
	        .depth  = 1,
	    },
	    .format = gli_to_vk_format_map.at(gli_cube.format()),
	    .levels = to_u32(gli_cube.levels()),
	};
}

ImageResource::ImageResource(Image &&image) :
    image_(std::move(image)){

    };

ImageResource::ImageResource(ImageResource &&rhs) :
    image_(std::move(rhs.image_)),
    p_view_(std::move(rhs.p_view_))
{
}

ImageResource::~ImageResource(){};

void ImageResource::create_view(const Device &device, vk::ImageViewCreateInfo &image_view_cinfo)
{
	p_view_ = std::make_unique<ImageView>(device, image_view_cinfo);
}

Image &ImageResource::get_image()
{
	return image_;
}

ImageView &ImageResource::get_view() const
{
	return *p_view_;
}
};        // namespace W3D