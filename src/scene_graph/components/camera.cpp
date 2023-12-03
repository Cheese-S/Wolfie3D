#include "camera.hpp"

#include "common/error.hpp"
#include "scene_graph/node.hpp"

namespace W3D::sg
{
Camera::Camera(const std::string &name) :
    Component(name)
{
}

std::type_index Camera::get_type()
{
	return typeid(Camera);
}

void Camera::set_node(Node &node)
{
	p_node_ = &node;
}

void Camera::set_pre_rotation(const glm::mat4 &pre_rotation)
{
	pre_rotation_ = pre_rotation;
}

glm::mat4 Camera::get_view()
{
	if (!p_node_)
	{
		throw std::runtime_error("Camera component is not attached to a node");
	}
	auto &T = p_node_->get_component<Transform>();
	return glm::inverse(T.get_world_M());
}

Node *Camera::get_node()
{
	return p_node_;
}

}        // namespace W3D::sg