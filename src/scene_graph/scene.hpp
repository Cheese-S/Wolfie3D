#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "common/glm_common.hpp"
#include "scene_graph/node.hpp"

namespace W3D::SceneGraph {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 uv;
};

class Node;
class Component;
class SubMesh;

class Scene {
   public:
    Scene() = default;
    Scene(const std::string &name);

    void add_node(std::unique_ptr<Node> &&pNode);
    void add_child(Node &child);
    void add_component(std::unique_ptr<Component> &&pComponent);
    void add_component_to_node(std::unique_ptr<Component> &&pComponent, Node &node);

    std::unique_ptr<Component> get_model(uint32_t index = 0);
    void set_root_node(Node &node);
    void set_nodes(std::vector<std::unique_ptr<Node>> &&nodes);
    Node *find_node(const std::string &name);

    template <typename T>
    void set_components(std::vector<std::unique_ptr<T>> pTs) {
        std::vector<std::unique_ptr<Component>> pComponents(pTs.size());
        std::transform(pTs.begin(), pTs.end(), pComponents.begin(),
                       [](std::unique_ptr<T> &pT) -> std::unique_ptr<Component> {
                           return std::unique_ptr<Component>(std::move(pT));
                       });
        set_components(typeid(T), std::move(pComponents));
    }
    void set_components(const std::type_index type,
                        std::vector<std::unique_ptr<Component>> pComponents);

    template <typename T>
    std::vector<T *> get_components() const {
        std::vector<T *> result;
        if (has_component(typeid(T))) {
            auto &scene_components = get_components(typeid(T));
            result.resize(scene_components.size());
            std::transform(scene_components.begin(), scene_components.end(), result.begin(),
                           [](const std::unique_ptr<Component> &component) -> T * {
                               return dynamic_cast<T *>(component.get());
                           });
        }
        return result;
    }

    const std::vector<std::unique_ptr<Component>> &get_components(
        const std::type_index &type) const;

    template <typename T>
    bool has_component() const {
        return has_component(typeid(T));
    }
    bool has_component(const std::type_index &type) const;

    Node &get_root_node();

   private:
    std::string name_;
    Node *root_ = nullptr;

    std::vector<std::unique_ptr<Node>> pNodes_;
    std::unordered_map<std::type_index, std::vector<std::unique_ptr<Component>>> pComponents_;
};

}  // namespace W3D::SceneGraph