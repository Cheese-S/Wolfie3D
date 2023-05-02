#pragma once
#include <string>

namespace W3D {

namespace SceneGraph {
class Scene;
class Node;
}  // namespace SceneGraph

std::string to_snake_case(const std::string &text);

SceneGraph::Node *add_free_camera(SceneGraph::Scene &scene, const std::string &node_name, int width,
                                  int height);

}  // namespace W3D
