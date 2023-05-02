#pragma once

#include "scene_graph/component.hpp"
#include "scene_graph/input_event.hpp"
#include "scene_graph/node.hpp"

namespace W3D::SceneGraph {

class Script : public Component {
   public:
    Script(const std::string &name = "");

    virtual ~Script() = default;
    virtual std::type_index get_type() override;

    virtual void update(float delta_time) = 0;
    virtual void process_input_event(const InputEvent &input_event);
    virtual void resize(uint32_t width, uint32_t height);
};

class NodeScript : public Script {
   public:
    NodeScript(Node &node, const std::string &name = "");
    virtual ~NodeScript() = default;
    Node &get_node();

   private:
    Node &node_;
};
}  // namespace W3D::SceneGraph