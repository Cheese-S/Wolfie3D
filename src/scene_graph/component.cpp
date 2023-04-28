#include "component.hpp"

namespace W3D::SceneGraph {
Component::Component(const std::string& name) : name_(name){};

const std::string& Component::get_name() const { return name_; }
}  // namespace W3D::SceneGraph