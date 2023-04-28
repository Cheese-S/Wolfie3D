#include "submesh.hpp"

#include "material.hpp"

namespace W3D::SceneGraph {
SubMesh::SubMesh(const std::string& name) : Component(name) {
}

std::type_index SubMesh::get_type() {
    return typeid(SubMesh);
}

void SubMesh::set_material(const Material& material) {
    pMaterial_ = &material;
}

const Material* SubMesh::get_material() const {
    return pMaterial_;
}

}  // namespace W3D::SceneGraph