#pragma once

#include <tiny_gltf.h>

#include <memory>

#include "common/glm_common.hpp"

namespace W3D {
class Device;

namespace DeviceMemory {
class Allocator;
}

namespace SceneGraph {
class Scene;
class Node;
class Camera;
class Image;
class Mesh;
class SubMesh;
class PBRMaterial;
class Sampler;
class Texture;
struct Vertex;
};  // namespace SceneGraph

class GLTFLoader {
   public:
    GLTFLoader(Device const &device);

    virtual ~GLTFLoader() = default;

    std::unique_ptr<SceneGraph::Scene> read_scene_from_file(const std::string &file_name,
                                                            int scene_index = -1);

   protected:
    std::unique_ptr<SceneGraph::Node> parse_node(const tinygltf::Node &gltf_node,
                                                 size_t index) const;
    std::unique_ptr<SceneGraph::Camera> parse_camera(const tinygltf::Camera &gltf_camera) const;
    std::unique_ptr<SceneGraph::Mesh> parse_mesh(const tinygltf::Mesh &gltf_mesh) const;
    std::unique_ptr<SceneGraph::PBRMaterial> parse_material(
        const tinygltf::Material &gltf_material) const;
    std::unique_ptr<SceneGraph::Image> parse_image(tinygltf::Image &gltf_image) const;
    std::unique_ptr<SceneGraph::Sampler> parse_sampler(const tinygltf::Sampler &gltf_sampler) const;
    std::unique_ptr<SceneGraph::Texture> parse_texture(const tinygltf::Texture &gltf_texture) const;
    std::unique_ptr<SceneGraph::SubMesh> parse_submesh_as_model(
        const tinygltf::Primitive &gltf_primitive) const;

    std::unique_ptr<SceneGraph::PBRMaterial> create_default_material();
    std::unique_ptr<SceneGraph::Sampler> create_default_sampler();
    std::unique_ptr<SceneGraph::Camera> create_default_camera();

    const Device &device_;
    tinygltf::Model gltf_model_;
    std::string model_path_;

   private:
    SceneGraph::Scene parse_scene(int scene_idx = -1);
};

}  // namespace W3D
