#pragma once

#include <tiny_gltf.h>

#include <memory>

#include "common/glm_common.hpp"

namespace W3D
{
class Device;

namespace DeviceMemory
{
class Allocator;
}

namespace sg
{
class Scene;
class Node;
class Camera;
class Image;
class Mesh;
class SubMesh;
class PBRMaterial;
class Sampler;
class Texture;
class AABB;
struct AnimationSampler;
struct AnimationChannel;
struct Vertex;
};        // namespace sg

struct ImageTransferInfo;

class GLTFLoader
{
  public:
	GLTFLoader(Device const &device);

	virtual ~GLTFLoader() = default;

	std::unique_ptr<sg::Scene> read_scene_from_file(const std::string &file_name,
	                                                int                scene_index = -1);

	std::unique_ptr<sg::SubMesh> read_model_from_file(const std::string &file_name, int mesh_idx);

  private:
	void      load_gltf_model(const std::string &file_name);
	sg::Scene parse_scene(int scene_idx = -1);

	void load_samplers() const;
	void load_images();
	void load_textures();
	void load_materials();
	void load_meshs();
	void load_cameras();
	void load_nodes(int scene_idx);
	void load_animations();
	void load_default_camera();

	std::vector<std::unique_ptr<sg::Node>> parse_nodes();
	std::unique_ptr<sg::Node>              parse_node(const tinygltf::Node &gltf_node,
	                                                  size_t                index) const;
	std::unique_ptr<sg::Camera>            parse_camera(const tinygltf::Camera &gltf_camera) const;
	std::unique_ptr<sg::Mesh>              parse_mesh(const tinygltf::Mesh &gltf_mesh) const;
	std::unique_ptr<sg::SubMesh>           parse_submesh(sg::Mesh *p_mesh, const tinygltf::Primitive &gltf_submesh) const;
	std::unique_ptr<sg::PBRMaterial>       parse_material(
	          const tinygltf::Material &gltf_material) const;
	std::unique_ptr<sg::Image>   parse_image(const tinygltf::Image &gltf_image);
	std::unique_ptr<sg::Sampler> parse_sampler(const tinygltf::Sampler &gltf_sampler) const;
	std::unique_ptr<sg::Texture> parse_texture(const tinygltf::Texture &gltf_texture) const;
	std::unique_ptr<sg::SubMesh> parse_submesh_as_model(
	    const tinygltf::Primitive &gltf_primitive) const;
	std::vector<sg::AnimationSampler> parse_animation_samplers(const tinygltf::Animation &gltf_animation);
	void                              parse_animation_input_data(const tinygltf::AnimationSampler &gltf_sampler, sg::AnimationSampler &sampler);
	void                              parse_animation_output_data(const tinygltf::AnimationSampler &gltf_sampler, sg::AnimationSampler &sampler);
	std::vector<sg::AnimationChannel> parse_animation_channels(const tinygltf::Animation &gltf_animation, std::vector<sg::Node *> p_nodes);

	std::unique_ptr<sg::PBRMaterial> create_default_material() const;
	std::unique_ptr<sg::Texture>     create_default_texture(sg::Sampler &default_sampler) const;
	std::unique_ptr<sg::Image>       create_default_texture_image() const;
	std::unique_ptr<sg::Sampler>     create_default_sampler() const;
	std::unique_ptr<sg::Camera>      create_default_camera() const;

	void             batch_upload_images() const;
	void             create_image_resource(sg::Image &image, size_t idx) const;
	void             append_textures_to_material(tinygltf::ParameterMap &parameter_map, std::vector<sg::Texture *> &p_textures, sg::PBRMaterial *p_material);
	size_t           get_submesh_vertex_count(const tinygltf::Primitive &submesh) const;
	void             update_parent_mesh_bound(sg::Mesh *p_mesh, const tinygltf::Primitive &gltf_submesh) const;
	tinygltf::Scene *pick_scene(int scene_idx);
	void             init_node_hierarchy(tinygltf::Scene *p_gltf_scene, std::vector<std::unique_ptr<sg::Node>> &p_nodes, sg::Node &root);
	void             init_scene_bound();

	template <typename T>
	const T *get_attr_data_ptr(const tinygltf::Primitive &submesh, const char *name) const
	{
		auto it = submesh.attributes.find(name);
		if (it == submesh.attributes.end())
		{
			return nullptr;
		}

		uint32_t accessor_id = it->second;
		assert(accessor_id < gltf_model_.accessors.size());
		const tinygltf::Accessor &accessor = gltf_model_.accessors[accessor_id];
		assert(accessor.bufferView < gltf_model_.bufferViews.size());
		const tinygltf::BufferView &buffer_view = gltf_model_.bufferViews[accessor.bufferView];
		assert(buffer_view.buffer < gltf_model_.buffers.size());
		const tinygltf::Buffer &buffer = gltf_model_.buffers[buffer_view.buffer];

		return reinterpret_cast<const T *>(&buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
	}

	const Device                  &device_;
	sg::Scene                     *p_scene_;
	tinygltf::Model                gltf_model_;
	std::string                    model_path_;
	std::vector<ImageTransferInfo> img_tinfos_;
};

}        // namespace W3D
