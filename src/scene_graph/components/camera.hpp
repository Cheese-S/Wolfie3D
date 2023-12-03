#pragma once

#include "common/glm_common.hpp"
#include "scene_graph/component.hpp"

namespace W3D::sg
{
class Node;

// Abstract Camera Component.
// Any class extending this needs to provide a way to generate projection matrix.
class Camera : public Component
{
  public:
	Camera(const std::string &name);
	virtual ~Camera() = default;
	virtual std::type_index get_type() override;

	void set_node(Node &node);
	void set_pre_rotation(const glm::mat4 &pre_rotation);

	virtual glm::mat4 get_projection() = 0;
	glm::mat4         get_view();
	Node             *get_node();

  private:
	Node     *p_node_{nullptr};
	glm::mat4 pre_rotation_{1.0f};
};
}        // namespace W3D::sg