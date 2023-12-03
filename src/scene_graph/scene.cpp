#include "scene.hpp"

#include "component.hpp"
#include "node.hpp"

namespace W3D::sg
{
Scene::Scene(const std::string &name) :
    name_(name)
{
}

// Add a node to the scene.
void Scene::add_node(std::unique_ptr<Node> &&pNode)
{
	p_nodes_.emplace_back(std::move(pNode));
}

// Add a child node to the root node.
void Scene::add_child(Node &child)
{
	root_->add_child(child);
}

// Add a component to the scene.
void Scene::add_component(std::unique_ptr<Component> &&pComponent)
{
	if (pComponent)
	{
		p_components_[pComponent->get_type()].push_back(std::move(pComponent));
	}
}

// Add a component to the node.
void Scene::add_component_to_node(std::unique_ptr<Component> &&pComponent, Node &node)
{
	if (pComponent)
	{
		node.set_component(*pComponent);
		p_components_[pComponent->get_type()].push_back(std::move(pComponent));
	}
}

// Helper function to add a vector of components.
void Scene::set_components(const std::type_index                   type,
                           std::vector<std::unique_ptr<Component>> pComponents)
{
	p_components_[type] = std::move(pComponents);
}

void Scene::set_root_node(Node &node)
{
	root_ = &node;
}

void Scene::set_nodes(std::vector<std::unique_ptr<Node>> &&nodes)
{
	p_nodes_ = std::move(nodes);
}

// Return raw pointers to the nodes.
std::vector<sg::Node *> Scene::get_nodes()
{
	std::vector<sg::Node *> res;
	res.reserve(p_nodes_.size());
	for (auto &p_node : p_nodes_)
	{
		res.push_back(p_node.get());
	}
	return res;
}

Node &Scene::get_root_node()
{
	return *root_;
}

// Access a node by index.
Node &Scene::get_node_by_index(int idx)
{
	assert(idx >= 0 && idx < p_nodes_.size());
	return *p_nodes_[idx].get();
}

// Get the scene's bound
AABB &Scene::get_bound()
{
	return bound_;
}

// Find a node by name.
Node *Scene::find_node(const std::string &name)
{
	for (auto &pNode : p_nodes_)
	{
		if (pNode->get_name() == name)
		{
			return pNode.get();
		}
	}

	return nullptr;
}

// Private function used by template get_components.
const std::vector<std::unique_ptr<Component>> &Scene::get_components(
    const std::type_index &type) const
{
	return p_components_.at(type);
}

bool Scene::has_component(const std::type_index &type) const
{
	return p_components_.count(type) > 0;
}
}        // namespace W3D::sg