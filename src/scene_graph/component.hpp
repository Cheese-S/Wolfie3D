#pragma once

#include <string>
#include <typeindex>

namespace W3D {
namespace SceneGraph {
class Component {
   public:
    Component() = default;
    virtual ~Component() = default;
    Component(const std::string& name);
    const std::string& get_name() const;
    virtual std::type_index get_type() = 0;

   private:
    std::string name_;
};

}  // namespace SceneGraph
}  // namespace W3D