#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "components/transform.hpp"

namespace W3D::SceneGraph {

class Node {
   public:
    Node(const size_t id, const std::string &name);
    void set_parent(Node &parent);
    void set_component(Component &component);
    const size_t get_id() const;
    const std::string &get_name() const;
    Node *get_parent() const;
    const std::vector<Node *> &get_children() const;
    template <class T>
    inline T &get_component() {
        return dynamic_cast<T &>(get_component(typeid(T)));
    }
    Component &get_component(const std::type_index index);

    template <class T>
    bool has_component() {
        return has_component(typeid(T));
    }
    bool has_component(const std::type_index index);

    void add_child(Node &child);

   private:
    size_t id_;
    std::string name_;
    Transform T_;
    Node *parent_{nullptr};
    std::vector<Node *> children_;
    std::unordered_map<std::type_index, Component *> components_;
};
}  // namespace W3D::SceneGraph