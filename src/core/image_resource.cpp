#include "image_resource.hpp"

#include <gli/gli.hpp>
#include <stb_image.h>

#include "common/file_utils.hpp"
#include "common/logging.hpp"
#include "core/device.hpp"
#include "core/image_view.hpp"

namespace W3D
{

uint8_t ImageResource::format_to_bits_per_pixel(vk::Format format)
{
	// TODO: add other formats
	static std::unordered_map<vk::Format, uint32_t> conversion_map{
	    {vk::Format::eR32G32B32A32Sfloat, 16},
	    {vk::Format::eR8G8B8A8Srgb, 4},
	};

	return conversion_map[format];
}

// Load an on disk 2d image.
ImageTransferInfo ImageResource::load_two_dim_image(const std::string &path)
{
	// We only use stb now, but we can add more later
	return stb_load(path);
};

// Create an EMPTY image resource with given metainfo.
// * The vkImage contains random bytes. It NEEDS to be updated.
ImageResource ImageResource::create_empty_two_dim_img_resrc(const Device &device, const ImageMetaInfo &meta)
{
	vk::ImageCreateInfo img_cinfo{
	    .imageType   = vk::ImageType::e2D,
	    .format      = meta.format,
	    .extent      = meta.extent,
	    .mipLevels   = meta.levels,
	    .arrayLayers = 1,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	    .sharingMode = vk::SharingMode::eExclusive,
	};

	Image img = device.get_device_memory_allocator().allocate_device_only_image(img_cinfo);

	vk::ImageViewCreateInfo view_cinfo = ImageView::two_dim_view_cinfo(img.get_handle(), img_cinfo.format, vk::ImageAspectFlagBits::eColor, meta.levels);
	ImageResource           resource   = ImageResource(std::move(img), ImageView(device, view_cinfo));

	return resource;
}

// using stb to load a 2d image.
ImageTransferInfo stb_load(const std::string &path)
{
	std::string extension = fu::get_file_extension(path);
	if (extension != "jpg" || extension != "png")
	{
		LOGE("Unsupported file type! W3D only supports loading jpg/png 2d images");
		abort();
	}

	// We require the image to be in rgba format.
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

	// Copy raw bytes to our array.
	std::vector<uint8_t> img_binary = {p_img_data, p_img_data + size};

	stbi_image_free(p_img_data);

	return {
	    .binary = std::move(img_binary),
	    .meta   = {
	          .extent = {
	              .width  = to_u32(width),
	              .height = to_u32(height),
	              .depth  = 1,
            },
	          .format = vk::Format::eR8G8B8A8Srgb,
	          .levels = 1,
        },
	};
}

// Load a cubic image using gli
ImageTransferInfo ImageResource::load_cubic_image(const std::string &path)
{
	return gli_load(path);
}

// Similar to the 2D case. Create an EMPTY image resource.
ImageResource ImageResource::create_empty_cubic_img_resrc(const Device &device, const ImageMetaInfo &meta)
{
	vk::ImageCreateInfo img_cinfo{
	    .flags       = vk::ImageCreateFlagBits::eCubeCompatible,
	    .imageType   = vk::ImageType::e2D,
	    .format      = meta.format,
	    .extent      = meta.extent,
	    .mipLevels   = meta.levels,
	    .arrayLayers = 6,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	    .sharingMode = vk::SharingMode::eExclusive,
	};

	Image img = device.get_device_memory_allocator().allocate_device_only_image(img_cinfo);

	vk::ImageViewCreateInfo view_cinfo = ImageView::cube_view_cinfo(img.get_handle(), img_cinfo.format, vk::ImageAspectFlagBits::eColor, meta.levels);
	ImageResource           resource   = ImageResource(std::move(img), ImageView(device, view_cinfo));

	return resource;
}

// using gli to load cubic image
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

	// Copy the raw bytes into our own array
	std::vector<std::uint8_t> img_binary{gli_cube.data<uint8_t>(), gli_cube.data<uint8_t>() + gli_cube.size()};

	return {
	    .binary = std::move(img_binary),
	    .meta   = {
	          .extent = {
	              .width  = to_u32(gli_cube.extent().x),
	              .height = to_u32(gli_cube.extent().y),
	              .depth  = 1,
            },
	          .format = gli_to_vk_format_map.at(gli_cube.format()),
	          .levels = to_u32(gli_cube.levels()),
        },
	};
}

// Create NULL image resource
ImageResource::ImageResource(const Device &device, std::nullptr_t nptr) :
    image_(device.get_device_memory_allocator().allocate_null_image()),
    view_(device, nptr)
{
}

// Constructor with right reference image and view.
// * The image and view will "move" their resource to this newly created ImageResource instance.
ImageResource::ImageResource(Image &&image, ImageView &&view) :
    image_(std::move(image)),
    view_(std::move(view)){

    };

// Move constructor
ImageResource::ImageResource(ImageResource &&rhs) :
    image_(std::move(rhs.image_)),
    view_(std::move(rhs.view_))
{
}

// Move assignment operator.
ImageResource &ImageResource::operator=(ImageResource &&rhs)
{
	image_ = std::move(rhs.image_);
	view_  = std::move(rhs.view_);
	return *this;
};

// Image and ImageView handles their own destruction.
ImageResource::~ImageResource(){};

Image &ImageResource::get_image()
{
	return image_;
}

const ImageView &ImageResource::get_view() const
{
	return view_;
}
};        // namespace W3D