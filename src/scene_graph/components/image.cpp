#include "image.hpp"

#include <stb_image.h>
#include <stb_image_resize.h>

#include "common/error.hpp"
#include "common/file_utils.hpp"
#include "core/device.hpp"


namespace W3D::SceneGraph {

std::unique_ptr<Image> Image::load(const std::string &name, const std::string &uri) {
    std::unique_ptr<Image> pImage = nullptr;
    auto data = W3D::fu::read_binary(uri);

    int width, height;
    int comp;
    int req_comp = 4;

    auto data_buffer = reinterpret_cast<const stbi_uc *>(data.data());
    auto data_size = static_cast<int>(data.size());

    auto raw_data = stbi_load_from_memory(data_buffer, data_size, &width, &height, &comp, req_comp);

    if (!raw_data) {
        throw std::runtime_error("Failed to load" + name + ": " + stbi_failure_reason());
    }

    pImage->data_ = {raw_data, raw_data + data_size};

    stbi_image_free(raw_data);

    pImage->format_ = vk::Format::eR8G8B8A8Srgb;
    pImage->mipmaps_[0].extent.width = static_cast<uint32_t>(width);
    pImage->mipmaps_[0].extent.height = height;
    pImage->mipmaps_[0].extent.depth = 1u;

    return pImage;
}

Image::Image(const std::string &name, std::vector<uint8_t> &&data, std::vector<Mipmap> &&mipmaps)
    : Component(name),
      data_(std::move(data)),
      format_(vk::Format::eR8G8B8A8Unorm),
      mipmaps_(std::move(mipmaps)) {
}

void Image::create_vk_image(const Device &device, vk::ImageViewType image_view_type,
                            vk::ImageViewCreateFlags flags) {
    assert(!pVkImage_);
    vk::ImageCreateInfo image_info;
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = format_;
    image_info.extent = get_extent();
    image_info.mipLevels = mipmaps_.size();
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    image_info.sharingMode = vk::SharingMode::eExclusive;

    pVkImage_ = device.get_allocator().allocate_device_only_image(image_info);

    view_ = device.createImageView(pVkImage_->handle(), format_, vk::ImageAspectFlagBits::eColor,
                                   static_cast<uint32_t>(mipmaps_.size()));
}

void Image::generate_mipmaps() {
    assert(mipmaps_.size() == 1);
    if (mipmaps_.size() > 1) {
        return;
    }

    auto extent = get_extent();
    auto next_width = std::max<uint32_t>(1u, extent.width / 2);
    auto next_height = std::max<uint32_t>(1u, extent.height / 2);
    auto channels = 4;
    auto next_size = next_width * next_height * channels;

    while (true) {
        auto old_size = static_cast<uint32_t>(data_.size());
        data_.resize(old_size + next_size);

        auto &prev_mipmap = mipmaps_.back();

        Mipmap next_mipmap;
        next_mipmap.level = prev_mipmap.level + 1;
        next_mipmap.offset = old_size;
        next_mipmap.extent = vk::Extent3D{next_width, next_height, 1u};

        stbir_resize_uint8(data_.data() + prev_mipmap.offset, prev_mipmap.extent.width,
                           prev_mipmap.extent.height, 0, data_.data() + next_mipmap.offset,
                           next_mipmap.extent.width, next_mipmap.extent.height, 0, channels);
        mipmaps_.emplace_back(next_mipmap);

        next_width = std::max<uint32_t>(1u, next_width / 2);
        next_height = std::max<uint32_t>(1u, next_height / 2);
        next_size = next_width * next_height * channels;

        if (next_width == 1 && next_height == 1) {
            break;
        }
    }
}

void Image::clear_data() {
    data_.clear();
}

std::type_index Image::get_type() {
    return typeid(Image);
}

const std::vector<uint8_t> &Image::get_data() const {
    return data_;
}

const vk::Extent3D &Image::get_extent() const {
    assert(!mipmaps_.empty());
    return mipmaps_[0].extent;
}

const uint32_t Image::get_layers() const {
    return layers_;
}

const std::vector<Mipmap> &Image::get_mipmaps() const {
    return mipmaps_;
}

const DeviceMemory::Image &Image::get_vk_image() const {
    assert(pVkImage_);
    return *pVkImage_;
}

const vk::raii::ImageView &Image::get_view() const {
    return view_;
}

}  // namespace W3D::SceneGraph