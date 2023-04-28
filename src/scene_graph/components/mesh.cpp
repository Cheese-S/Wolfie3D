#include "mesh.hpp"

namespace W3D::SceneGraph {

Mesh::Mesh(const std::string &name) : Component(name){};

void Mesh::add_submesh(SubMesh &submesh) {
    submeshes_.push_back(&submesh);
}

void Mesh::add_node(Node &node) {
    nodes_.push_back(&node);
}

std::type_index Mesh::get_type() {
    return typeid(Mesh);
}

const AABB &Mesh::get_bounds() const {
    return bounds_;
}

const std::vector<SubMesh *> &Mesh::get_submeshes() const {
    return submeshes_;
}

const std::vector<Node *> &Mesh::get_nodes() const {
    return nodes_;
}
}  // namespace W3D::SceneGraph