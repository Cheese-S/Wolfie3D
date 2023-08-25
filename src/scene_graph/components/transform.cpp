#include "transform.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include "scene_graph/node.hpp"

namespace W3D::sg
{
Transform::Transform(Node &node) :
    node_(node)
{
}

std::type_index Transform::get_type()
{
	return typeid(Transform);
}

glm::mat4 Transform::get_world_M()
{
	if (!need_update_)
	{
		return world_M_;
	}
	world_M_ = get_local_M();

	auto parent = node_.get_parent();

	if (parent)
	{
		auto &transform = parent->get_component<Transform>();
		world_M_        = transform.get_world_M() * world_M_;
	}
	need_update_ = false;
	return world_M_;
}

glm::mat4 Transform::get_local_M()
{
	return glm::translate(glm::mat4(1.0), translation_) * glm::mat4_cast(rotation_) *
	       glm::scale(glm::mat4(1.0), scale_);
}

glm::vec3 Transform::get_scale()
{
	return scale_;
}

glm::vec3 Transform::get_translation()
{
	return translation_;
}

glm::quat Transform::get_rotation()
{
	return rotation_;
}

void Transform::set_tranlsation(const glm::vec3 &translation)
{
	translation_ = translation;
	invalidate_world_M();
}
void Transform::set_rotation(const glm::quat &rotation)
{
	rotation_ = rotation;
	invalidate_world_M();
}

void Transform::set_scale(const glm::vec3 &scale)
{
	scale_ = scale;
	invalidate_world_M();
}

void Transform::set_world_M(const glm::mat4 &world_M)
{
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(world_M, scale_, rotation_, translation_, skew, perspective);
	invalidate_world_M();
}

void Transform::invalidate_world_M()
{
	need_update_ = true;
}

}        // namespace W3D::sg