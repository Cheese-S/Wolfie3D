#pragma once

#include <memory>

#include "common/glm_common.hpp"

#include "core/descriptor_allocator.hpp"
#include "core/image_resource.hpp"
#include "core/sampler.hpp"

namespace W3D
{
namespace sg
{
class SubMesh;
}
class Device;

class GraphicsPipeline;
class RenderPass;
class Framebuffer;
class CommandBuffer;

// PBRTexture manages its own lifetime.
// * It is NOT owned by a scene.
struct PBRTexture
{
	PBRTexture(ImageResource &&resource, Sampler &&sampler) :
	    resource(std::move(resource)),
	    sampler(std::move(sampler)){};
	ImageResource resource;
	Sampler       sampler;
};

// POD struct for all the PBR resource.
struct PBR
{
	PBR();
	~PBR();

	PBR(PBR &&rhs)            = default;
	PBR &operator=(PBR &&rhs) = default;

	std::unique_ptr<PBRTexture>  p_background;
	std::unique_ptr<PBRTexture>  p_irradiance;
	std::unique_ptr<PBRTexture>  p_prefilter;
	std::unique_ptr<PBRTexture>  p_brdf_lut;
	std::unique_ptr<sg::SubMesh> p_box;
};

// Responsible for baking IBL resources.
// To get a better understanding about PBR and IBL, refer to https://learnopengl.com/PBR/Theory
class PBRBaker
{
  public:
	static const uint32_t IRRADIANCE_DIMENSION;
	static const uint32_t PREFILTER_DIMENSION;
	static const uint32_t BRDF_LUT_DIMENSION;

	PBRBaker(Device &device);

	PBR bake();

  private:
	static const std::vector<glm::mat4> CUBE_FACE_MATRIXS;

	void load_background();
	void load_cube_model();
	void prepare_prefilter();
	void prepare_irradiance();
	void prepare_brdf_lut();
	void bake_irradiance(ImageMetaInfo &cube_meta);
	void bake_prefilter(ImageMetaInfo &cube_meta);
	void bake_brdf_lut();

	void draw_box(CommandBuffer &cmd_buf);
	void transfer_from_src_to_texture(CommandBuffer &cmd_buf, ImageResource &src, PBRTexture &texture, vk::ImageCopy copy_region);

	RenderPass                  create_color_only_renderpass(vk::Format format, vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined, vk::ImageLayout final_layout = vk::ImageLayout::eColorAttachmentOptimal);
	ImageResource               create_transfer_src(vk::Extent3D extent, vk::Format format);
	Framebuffer                 create_square_framebuffer(const RenderPass &render_pass, const ImageView &view, uint32_t dimension);
	GraphicsPipeline            create_graphics_pipeline(RenderPass &render_pass, vk::PipelineLayoutCreateInfo &ppl_layout_cinfo, const char *vert_shader_name, const char *frag_shader_name);
	DescriptorAllocation        allocate_texture_descriptor(PBRTexture &texture);
	std::unique_ptr<PBRTexture> create_empty_cube_texture(ImageMetaInfo &cube_meta);
	ImageResource               create_empty_cubic_img_resource(ImageMetaInfo &img_tinfo);
	void                        create_brdf_lut_texture();

	Device         &device_;
	PBR             result_;
	DescriptorState desc_state_;
};
}        // namespace W3D
