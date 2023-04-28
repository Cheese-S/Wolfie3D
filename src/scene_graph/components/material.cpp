#include "material.hpp"

namespace W3D::SceneGraph {
Material::Material(const std::string &name) : Component(name) {
}

std::type_index Material::get_type() {
    return typeid(Material);
}

}  // namespace W3D::SceneGraph