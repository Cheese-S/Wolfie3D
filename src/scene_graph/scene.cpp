#include "scene.hpp"

#include "component.hpp"
#include "node.hpp"

namespace W3D::SceneGraph {
Scene::Scene(const std::string &name) : name_(name) {
}

void Scene::add_node(std::unique_ptr<Node> &&pNode) {
    pNodes_.emplace_back(std::move(pNode));
}

void Scene::add_child(Node &child) {
    root_->add_child(child);
}

void Scene::add_component(std::unique_ptr<Component> &&pComponent) {
    if (pComponent) {
        pComponents_[pComponent->get_type()].push_back(std::move(pComponent));
    }
}

void Scene::add_component_to_node(std::unique_ptr<Component> &&pComponent, Node &node) {
    if (pComponent) {
        node.set_component(*pComponent);
        pComponents_[pComponent->get_type()].push_back(std::move(pComponent));
    }
}

void Scene::set_components(const std::type_index type,
                           std::vector<std::unique_ptr<Component>> pComponents) {
    pComponents_[type] = std::move(pComponents);
}

void Scene::set_root_node(Node &node) {
    root_ = &node;
}

void Scene::set_nodes(std::vector<std::unique_ptr<Node>> &&nodes) {
    pNodes_ = std::move(nodes);
}

Node &Scene::get_root_node() {
    return *root_;
}

Node *Scene::find_node(const std::string &name) {
    for (auto &pNode : pNodes_) {
        if (pNode->get_name() == name) {
            return pNode.get();
        }
    }

    return nullptr;
}

const std::vector<std::unique_ptr<Component>> &Scene::get_components(
    const std::type_index &type) const {
    return pComponents_.at(type);
}

bool Scene::has_component(const std::type_index &type) const {
    return pComponents_.count(type) > 0;
}
}  // namespace W3D::SceneGraph