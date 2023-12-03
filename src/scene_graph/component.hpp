#pragma once

#include <string>
#include <typeindex>

namespace W3D
{
namespace sg
{

// An abstract component class.
class Component
{
  public:
	Component()          = default;
	virtual ~Component() = default;
	Component(const std::string &name);

	const std::string      &get_name() const;
	virtual std::type_index get_type() = 0;        // This function allows us to get list of components by type.

  private:
	std::string name_;
};

}        // namespace sg
}        // namespace W3D