#pragma once

#include <vector>

#include "aabb.hpp"
#include "scene_graph/component.hpp"
#include "scene_graph/node.hpp"

namespace W3D::SceneGraph {
class SubMesh;
class Mesh : public Component {
   public:
    Mesh(const std::string &name);
    virtual ~Mesh() = default;

    void add_submesh(SubMesh &submesh);
    void add_node(Node &node);

    virtual std::type_index get_type() override;
    const AABB &get_bounds() const;
    const std::vector<SubMesh *> &get_submeshes() const;
    const std::vector<Node *> &get_nodes() const;

   private:
    AABB bounds_;
    std::vector<SubMesh *> submeshes_;
    std::vector<Node *> nodes_;
};
}  // namespace W3D::SceneGraph