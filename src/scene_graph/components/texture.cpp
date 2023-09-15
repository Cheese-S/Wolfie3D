#include "texture.hpp"

#include "common/error.hpp"

namespace W3D::sg
{
Texture::Texture(const std::string &name) :
    Component(name)
{
}

std::type_index Texture::get_type()
{
	return typeid(Texture);
}

}        // namespace W3D::sg