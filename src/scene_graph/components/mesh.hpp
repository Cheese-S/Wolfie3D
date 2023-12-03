#pragma once

#include <vector>

#include "aabb.hpp"
#include "scene_graph/component.hpp"
#include "scene_graph/node.hpp"

namespace W3D::sg
{
class SubMesh;

// Mesh Component.
// Has an AABB, pointers to its submeshes and pointers to nodes that refers to it.
class Mesh : public Component
{
  public:
	Mesh(const std::string &name);
	virtual ~Mesh() = default;

	void add_submesh(SubMesh &submesh);
	void add_node(Node &node);

	virtual std::type_index       get_type() override;
	const AABB                   &get_bounds() const;
	AABB                         &get_mut_bounds();
	const std::vector<SubMesh *> &get_p_submeshs() const;
	const std::vector<Node *>    &get_p_nodes() const;

  private:
	AABB                   bounds_;
	std::vector<SubMesh *> p_submeshs;
	std::vector<Node *>    p_nodes;
};
}        // namespace W3D::sg