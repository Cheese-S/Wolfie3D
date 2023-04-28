#pragma once

#include "common/common.hpp"
#include "scene_graph/component.hpp"

namespace W3D::SceneGraph {
class Sampler : public Component {
   public:
    Sampler(const std::string &name, vk::raii::Sampler &&vk_sampler);
    Sampler(Sampler &&other) = default;

    virtual ~Sampler() = default;
    virtual std::type_index get_type() override;

    vk::raii::Sampler vk_sampler_ = nullptr;
};
}  // namespace W3D::SceneGraph