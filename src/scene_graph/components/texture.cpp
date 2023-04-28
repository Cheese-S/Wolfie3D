#include "texture.hpp"

#include "common/error.hpp"

namespace W3D::SceneGraph {
Texture::Texture(const std::string& name) : Component(name) {
}

std::type_index Texture::get_type() {
    return typeid(Texture);
};

void Texture::set_image(Image& image) {
    pImage_ = &image;
}

void Texture::set_sampler(Sampler& sampler) {
    pSampler_ = &sampler;
}

Image* Texture::get_image() {
    return pImage_;
}

Sampler* Texture::get_sampler() {
    assert(pSampler_);
    return pSampler_;
}

}  // namespace W3D::SceneGraph