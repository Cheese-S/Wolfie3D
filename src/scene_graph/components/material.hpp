#pragma once

#include <unordered_map>

#include "common/glm_common.hpp"
#include "scene_graph/component.hpp"

namespace W3D::SceneGraph {
enum class AlphaMode { Opaque, Mask, Blend };
class Texture;
class Material : public Component {
   public:
    Material(const std::string &name);
    Material(Material &&other) = default;
    virtual ~Material() = default;
    virtual std::type_index get_type() override;

    std::unordered_map<std::string, Texture *> textures_;

    glm::vec3 emissive_{0.0f, 0.0f, 0.0f};
    bool is_double_sided{false};
    float alpha_cutoff_{0.5f};
    AlphaMode alpha_mode_{AlphaMode::Opaque};
};
}  // namespace W3D::SceneGraph