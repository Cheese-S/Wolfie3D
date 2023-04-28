#include "sampler.hpp"

namespace W3D::SceneGraph {
Sampler::Sampler(const std::string &name, vk::raii::Sampler &&vk_sampler)
    : Component(name), vk_sampler_(std::move(vk_sampler)){};

std::type_index Sampler::get_type() {
    return typeid(Sampler);
}
}  // namespace W3D::SceneGraph
