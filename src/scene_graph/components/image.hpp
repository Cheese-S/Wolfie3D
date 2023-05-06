#pragma once

#include <stdint.h>

#include "common/common.hpp"
#include "core/memory.hpp"
#include "scene_graph/component.hpp"

namespace W3D::SceneGraph {
struct Mipmap {
    uint32_t level = 0;
    uint32_t offset = 0;
    vk::Extent3D extent = {0, 0, 0};
};

class Image : public Component {
   public:
    static std::unique_ptr<Image> load(const std::string &name, const std::string &uri);
    static std::unique_ptr<Image> load_cubemap(const std::string &name, const std::string &uri);

    Image(const std::string &name, std::vector<uint8_t> &&data = {},
          std::vector<Mipmap> &&mipmaps = {});
    virtual ~Image() = default;

    void generate_mipmaps();
    void create_vk_image(const Device &device,
                         vk::ImageViewType image_view_type = vk::ImageViewType::e2D,
                         vk::ImageViewCreateFlags flags = {});
    void clear_data();

    virtual std::type_index get_type() override;
    const std::vector<uint8_t> &get_data() const;
    const vk::Extent3D &get_extent() const;
    const uint32_t get_layers() const;
    const std::vector<Mipmap> &get_mipmaps() const;
    const DeviceMemory::Image &get_vk_image() const;
    const vk::raii::ImageView &get_view() const;
    vk::Format get_format() const;

   protected:
    std::vector<uint8_t> data_;
    vk::Format format_ = vk::Format::eUndefined;
    uint32_t layers_ = 1;
    std::vector<Mipmap> mipmaps_;
    std::unique_ptr<DeviceMemory::Image> pVkImage_;
    vk::raii::ImageView view_{nullptr};
};
}  // namespace W3D::SceneGraph