#include "material.hpp"

namespace W3D::sg
{
Material::Material(const std::string &name) :
    Component(name)
{
}

std::type_index Material::get_type()
{
	return typeid(Material);
}

}        // namespace W3D::sg