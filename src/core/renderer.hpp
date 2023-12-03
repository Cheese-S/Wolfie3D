#pragma once

#include "common/timer.hpp"
#include "common/vk_common.hpp"

#include "command_buffer.hpp"
#include "core/image_resource.hpp"
#include "core/sampler.hpp"
#include "device_memory/buffer.hpp"
#include "pbr_baker.hpp"
#include "scene_graph/components/skin.hpp"
#include "sync_objects.hpp"

namespace W3D
{

namespace sg
{
class PBRMaterial;
class Texture;
class Camera;
}        // namespace sg

class Window;
class Instance;
class PhysicalDevice;
class Device;
class Swapchain;
class RenderPass;
class SwapchainFramebuffer;
class PipelineResource;

struct DescriptorState;
struct Event;

// This class is the center of all operations.
// It handles the creation of vulkan, scene, and PBR resources.
class Renderer
{
  public:
	Renderer();
	~Renderer();

	void start();
	void process_event(const Event &event);

  private:
	static const uint32_t NUM_INFLIGHT_FRAMES;        // We use two inflight frames to avoid idling GPU.

	// POD struct containing all resource that needs to be seperated by frame.
	struct FrameResource
	{
		CommandBuffer     cmd_buf;
		Buffer            camera_buf;
		Buffer            joint_buf;
		Semaphore         image_avaliable_semaphore;
		Semaphore         render_finished_semaphore;
		Fence             in_flight_fence;
		vk::DescriptorSet pbr_set;
		vk::DescriptorSet skybox_set;
	};

	// POD struct to contain the graphics pipeline and descriptor layouts.
	struct PipelineResource
	{
		std::unique_ptr<GraphicsPipeline>      p_pl;
		std::array<vk::DescriptorSetLayout, 4> desc_layout_ring;
	};

	enum DescriptorRingAccessor
	{
		eGlobal   = 0,
		eMaterial = 1,
	};

	// Uniform Object for animation;
	struct JointUBO
	{
		glm::mat4 joint_Ms[sg::Skin::MAX_NUM_JOINTS];
		float     is_skinned;        // boolean value that tells the shader whether the current submesh is skinned. We use a float b/c alignment.
	};

	// Uniform Object for camera matrices.
	struct CameraUBO
	{
		glm::mat4 proj_view;
		glm::vec3 cam_pos;
	};

	// Push constant object for skybox pipeline.
	struct SkyboxPCO
	{
		glm::mat4 proj;
		glm::mat4 view;
	};

	// Push constant object for pbr pipeline.
	struct PBRPCO
	{
		glm::mat4 model;
		glm::vec4 base_color;
		glm::vec4 metallic_roughness;
		uint32_t  material_flag;
	};

	// High level operations.
	void main_loop();
	void update();
	void render_frame();

	// Mid level operations called during render_frame()
	uint32_t sync_acquire_next_image();
	void     sync_submit_commands();
	void     sync_present(uint32_t img_idx);
	void     record_draw_commands(uint32_t img_idx);

	// Low level operations called druing render_frame()
	void update_camera_ubo();
	void set_dynamic_states(CommandBuffer &cmd_buf);
	void begin_render_pass(CommandBuffer &cmd_buf, vk::Framebuffer framebuffer);
	void draw_scene(CommandBuffer &cmd_buf);
	void draw_skybox(CommandBuffer &cmd_buf);
	void draw_node(CommandBuffer &cmd_buf, sg::Node &node);
	void draw_submesh(CommandBuffer &cmd_buf, sg::SubMesh &submesh);
	void bind_material(CommandBuffer &cmd_buf, const sg::PBRMaterial &material, PBRPCO &pco);
	void bind_skin(CommandBuffer &cmd_buf, const sg::Skin &skin);
	void disable_skin(CommandBuffer &cmd_buf);

	// Misc. Functions.
	void           resize();
	FrameResource &get_current_frame_resource();

	// Resource creation functions.
	void load_scene(const char *scene_name);
	void create_pbr_resources();
	void create_rendering_resources();
	void create_frame_resources();
	void create_descriptor_resources();
	void create_skybox_desc_resources();
	void create_pbr_desc_resources();
	void create_materials_desc_resources();
	void create_render_pass();
	void create_pipeline_resources();

	// Vulkan and Scene Graph State.
	std::unique_ptr<Window>               p_window_;
	std::unique_ptr<Instance>             p_instance_;
	std::unique_ptr<PhysicalDevice>       p_physical_device_;
	std::unique_ptr<Device>               p_device_;
	std::unique_ptr<Swapchain>            p_swapchain_;
	std::unique_ptr<RenderPass>           p_render_pass_;
	std::unique_ptr<SwapchainFramebuffer> p_sframe_buffer_;
	std::unique_ptr<DescriptorState>      p_descriptor_state_;
	std::unique_ptr<CommandPool>          p_cmd_pool_;
	std::unique_ptr<sg::Scene>            p_scene_;
	sg::Node                             *p_camera_node_ = nullptr;

	// Renderer State
	Timer                      timer_;
	uint32_t                   frame_idx_ = 0;
	std::vector<FrameResource> frame_resources_;
	PipelineResource           skybox_;
	PipelineResource           pbr_;
	PBR                        baked_pbr_;
	bool                       is_window_resized_ = false;
};
}        // namespace W3D