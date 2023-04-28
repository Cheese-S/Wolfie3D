#include "pbr_material.hpp"

namespace W3D::SceneGraph {
PBRMaterial::PBRMaterial(const std::string& name) : Material(name) {
}

std::type_index PBRMaterial::get_type() {
    return typeid(PBRMaterial);
}
}  // namespace W3D::SceneGraph