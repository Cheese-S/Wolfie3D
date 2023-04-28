#pragma once

#include "common/glm_common.hpp"
#include "scene_graph/components/material.hpp"

namespace W3D::SceneGraph {
class PBRMaterial : public Material {
   public:
    PBRMaterial(const std::string &name);
    virtual ~PBRMaterial() = default;
    virtual std::type_index get_type() override;

    glm::vec4 base_color_factor_{0.0f, 0.0f, 0.0f, 0.0f};
    float metallic_factor{0.0f};
    float roughness_factor{0.0f};
};
}  // namespace W3D::SceneGraph