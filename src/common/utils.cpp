#include "utils.hpp"

#include <sstream>

#include "core/device.hpp"
#include "scene_graph/components/camera.hpp"
#include "scene_graph/components/image.hpp"
#include "scene_graph/components/texture.hpp"
#include "scene_graph/node.hpp"
#include "scene_graph/scene.hpp"
#include "scene_graph/scripts/free_camera.hpp"

namespace W3D {
std::string to_snake_case(const std::string &text) {
    std::stringstream result;
    for (const auto c : text) {
        if (std::isalpha(c)) {
            if (std::isspace(c)) {
                result << "_";
            } else {
                if (std::isupper(c)) {
                    result << "_";
                }

                result << static_cast<char>(std::tolower(c));
            }
        } else {
            result << c;
        }
    }

    return result.str();
}

SceneGraph::Node *add_free_camera(SceneGraph::Scene &scene, const std::string &node_name, int width,
                                  int height) {
    auto camera_node = scene.find_node(node_name);

    if (!camera_node) {
        camera_node = scene.find_node("default_camera");
    }

    if (!camera_node) {
        throw std::runtime_error("Unable to find a camera node!");
    }

    if (!camera_node->has_component<SceneGraph::Camera>()) {
        throw std::runtime_error("No camera component found");
    }

    auto free_camera_script = std::make_unique<SceneGraph::FreeCamera>(*camera_node);

    free_camera_script->resize(width, height);
    scene.add_component_to_node(std::move(free_camera_script), *camera_node);

    return camera_node;
}

}  // namespace W3D