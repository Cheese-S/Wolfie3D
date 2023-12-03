#include "pbr_baker.hpp"

#include "gltf_loader.hpp"

#include "common/error.hpp"

#include "common/file_utils.hpp"
#include "core/command_buffer.hpp"
#include "core/device.hpp"
#include "core/device_memory/buffer.hpp"
#include "core/framebuffer.hpp"
#include "core/graphics_pipeline.hpp"
#include "core/image_view.hpp"
#include "core/render_pass.hpp"

#include "scene_graph/components/submesh.hpp"

#include "renderdoc_app.h"

W3D_DISABLE_WARNINGS()
#include <libloaderapi.h>
#include <minwindef.h>
W3D_ENABLE_WARNINGS()

RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

namespace W3D
{

PBR::PBR(){};

PBR::~PBR(){};

// Default resolutions for all PBR textures.
const uint32_t PBRBaker::IRRADIANCE_DIMENSION = 64;
const uint32_t PBRBaker::PREFILTER_DIMENSION  = 512;
const uint32_t PBRBaker::BRDF_LUT_DIMENSION   = 512;

const std::vector<glm::mat4> PBRBaker::CUBE_FACE_MATRIXS = {
    // POSITIVE_X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                glm::radians(180.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                glm::radians(180.0f),
                glm::vec3(1.0f, 0.0f, 0.0f)),
    // POSITIVE_Y
    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_Y
    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
    // POSITIVE_Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
    // NEGATIVE_Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
};

// Create the PBRBaker and init renderdoc (only used for debugging).
PBRBaker::PBRBaker(Device &device) :
    device_(device),
    desc_state_(device)
{
	if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI =
		    (pRENDERDOC_GetAPI) GetProcAddress(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **) &rdoc_api);
		assert(ret == 1);
	}
	load_cube_model();
	load_background();
}

// Bake all the IBL resources.
PBR PBRBaker::bake()
{
	prepare_irradiance();
	prepare_prefilter();
	prepare_brdf_lut();
	return std::move(result_);
}

// Load a texture cube model. We will render irradiance and prefilter using it.
void PBRBaker::load_cube_model()
{
	GLTFLoader loader(device_);
	result_.p_box = loader.read_model_from_file("2.0/BoxTextured/gltf/BoxTextured.gltf", 0);
}

// Load a HDR cubemap.
// * We hardcoded the HDR cubemap. But it can be replaced with other .dds HDR cubemap.
void PBRBaker::load_background()
{
	std::string       path      = fu::compute_abs_path(fu::FileType::eImage, "papermill.dds");
	ImageTransferInfo img_tinfo = ImageResource::load_cubic_image(path);
	ImageResource     resource  = ImageResource::create_empty_cubic_img_resrc(device_, img_tinfo.meta);

	CommandBuffer cmd_buf     = device_.begin_one_time_buf();
	Buffer        staging_buf = device_.get_device_memory_allocator().allocate_staging_buffer(img_tinfo.binary.size());
	staging_buf.update(img_tinfo.binary);

	cmd_buf.set_image_layout(resource, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer);

	cmd_buf.update_image(resource, staging_buf);

	cmd_buf.set_image_layout(resource, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader);

	device_.end_one_time_buf(cmd_buf);

	vk::SamplerCreateInfo sampler_cinfo = Sampler::linear_clamp_cinfo(device_.get_physical_device(), img_tinfo.meta.levels);

	result_.p_background = std::make_unique<PBRTexture>(std::move(resource), Sampler(device_, sampler_cinfo));
}

// Create and bake the irradiance texture.
void PBRBaker::prepare_irradiance()
{
	ImageMetaInfo cube_meta{
	    .extent = {
	        .width  = IRRADIANCE_DIMENSION,
	        .height = IRRADIANCE_DIMENSION,
	        .depth  = 1,
	    },
	    .format = vk::Format::eR32G32B32A32Sfloat,
	    .levels = max_mip_levels(IRRADIANCE_DIMENSION, IRRADIANCE_DIMENSION),
	};
	result_.p_irradiance = create_empty_cube_texture(cube_meta);
	if (rdoc_api)
		rdoc_api->StartFrameCapture(NULL, NULL);
	bake_irradiance(cube_meta);
	if (rdoc_api)
		rdoc_api->EndFrameCapture(NULL, NULL);
};

// Bake the irradiance texture.
// The irradiance texture is a convoluted cubemap that allows us to query the irradiance using a direction.
// The position is assumed to be fixed. We store the irradiance at postion p with direction N at cubemap's textel at N.
void PBRBaker::bake_irradiance(ImageMetaInfo &cube_meta)
{
	RenderPass           render_pass        = create_color_only_renderpass(cube_meta.format);
	ImageResource        transfer_src_resrc = create_transfer_src(cube_meta.extent, cube_meta.format);
	Framebuffer          framebuffer        = create_square_framebuffer(render_pass, transfer_src_resrc.get_view(), cube_meta.extent.width);
	DescriptorAllocation desc_allocation    = allocate_texture_descriptor(*result_.p_background);

	vk::PushConstantRange push_constant_range{
	    .stageFlags = vk::ShaderStageFlagBits::eVertex,
	    .offset     = 0,
	    .size       = sizeof(glm::mat4),
	};

	vk::PipelineLayoutCreateInfo pl_layout_cinfo{
	    .setLayoutCount         = 1,
	    .pSetLayouts            = &desc_allocation.set_layout,
	    .pushConstantRangeCount = 1,
	    .pPushConstantRanges    = &push_constant_range,
	};

	GraphicsPipeline pl = create_graphics_pipeline(render_pass, pl_layout_cinfo, "irradiance.vert.spv", "irradiance.frag.spv");

	std::array<vk::ClearValue, 1> clear_values = {
	    std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f},
	};
	vk::RenderPassBeginInfo pass_begin_info{
	    .renderPass  = render_pass.get_handle(),
	    .framebuffer = framebuffer.get_handle(),
	    .renderArea  = {
	         .extent = {
	             .width  = cube_meta.extent.width,
	             .height = cube_meta.extent.height,
            }},
	    .clearValueCount = to_u32(clear_values.size()),
	    .pClearValues    = clear_values.data(),
	};

	CommandBuffer     bake_buf        = device_.begin_one_time_buf();
	vk::CommandBuffer bake_buf_handle = bake_buf.get_handle();
	vk::Viewport      viewport{
	         .x = 0,
	         .y = 0,
    };
	vk::Rect2D scissor{
	    .offset = {
	        .x = 0,
	        .y = 0,
	    },
	    .extent = {
	        .width  = cube_meta.extent.width,
	        .height = cube_meta.extent.height,
	    },
	};

	bake_buf.set_image_layout(result_.p_irradiance->resource, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer);

	uint32_t  img_width  = cube_meta.extent.width;
	uint32_t  img_height = cube_meta.extent.height;
	glm::mat4 pco        = glm::mat4(1.0f);

	// We need to render to all 6 faces and all mipmap levels.
	for (uint32_t m = 0; m < cube_meta.levels; m++)
	{
		img_width  = std::max(1u, cube_meta.extent.width >> m);
		img_height = std::max(1u, cube_meta.extent.height >> m);
		for (uint32_t f = 0; f < 6; f++)
		{
			// We render to f face at m level.
			// Once we finished rendering it, convert the format for transfer src.
			// Transfer the render result from the framebuffer to the PBRTexture.
			// Convert the format back for rendering.
			// Repeat for all f and m.
			viewport.width  = img_width;
			viewport.height = img_height;
			pco             = glm::perspective(glm::pi<float>() / 2.0f, 1.0f, 0.1f, 512.0f) * CUBE_FACE_MATRIXS[f];
			bake_buf_handle.setViewport(0, viewport);
			bake_buf_handle.setScissor(0, scissor);
			bake_buf_handle.beginRenderPass(pass_begin_info, vk::SubpassContents::eInline);
			bake_buf_handle.pushConstants<glm::mat4>(pl.get_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0, pco);
			bake_buf_handle.bindPipeline(vk::PipelineBindPoint::eGraphics, pl.get_handle());
			bake_buf_handle.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pl.get_pipeline_layout(), 0, desc_allocation.set, {});
			draw_box(bake_buf);
			bake_buf_handle.endRenderPass();
			bake_buf.set_image_layout(transfer_src_resrc, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);

			vk::ImageCopy copy_region = {
			    .srcSubresource = {
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .mipLevel       = 0,
			        .baseArrayLayer = 0,
			        .layerCount     = 1,
			    },
			    .dstSubresource{
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .mipLevel       = m,
			        .baseArrayLayer = f,
			        .layerCount     = 1,
			    },
			    .extent = {
			        .width  = img_width,
			        .height = img_height,
			        .depth  = 1,
			    },
			};

			bake_buf_handle.copyImage(transfer_src_resrc.get_image().get_handle(), vk::ImageLayout::eTransferSrcOptimal, result_.p_irradiance->resource.get_image().get_handle(), vk::ImageLayout::eTransferDstOptimal, copy_region);
			bake_buf.set_image_layout(transfer_src_resrc, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eColorAttachmentOptimal);
		}
	}

	bake_buf.set_image_layout(result_.p_irradiance->resource, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	device_.end_one_time_buf(bake_buf);
}

// Create the prefilter PBRTexture and bake it.
void PBRBaker::prepare_prefilter()
{
	ImageMetaInfo cube_meta{
	    .extent = {
	        .width  = PREFILTER_DIMENSION,
	        .height = PREFILTER_DIMENSION,
	        .depth  = 1,
	    },
	    .format = vk::Format::eR16G16B16A16Sfloat,
	    .levels = max_mip_levels(PREFILTER_DIMENSION, PREFILTER_DIMENSION),
	};
	result_.p_prefilter = create_empty_cube_texture(cube_meta);
	if (rdoc_api)
		rdoc_api->StartFrameCapture(NULL, NULL);
	bake_prefilter(cube_meta);
	if (rdoc_api)
		rdoc_api->EndFrameCapture(NULL, NULL);
}

// Bake the prefilter texture.
// Similar to Irradiance map, we allow shader to use a direction N to sample from the texture.
// The position is assumed to be fixed. We store the prefilter at postion p with direction N at cubemap's textel at N.
void PBRBaker::bake_prefilter(ImageMetaInfo &cube_meta)
{
	RenderPass           render_pass        = create_color_only_renderpass(cube_meta.format);
	ImageResource        transfer_src_resrc = create_transfer_src(cube_meta.extent, cube_meta.format);
	Framebuffer          framebuffer        = create_square_framebuffer(render_pass, transfer_src_resrc.get_view(), cube_meta.extent.width);
	DescriptorAllocation desc_allocation    = allocate_texture_descriptor(*result_.p_background);

	struct PCO
	{
		glm::mat4 proj;
		float     roughness;
	} pco;
	vk::PushConstantRange push_constant_range{
	    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
	    0,
	    sizeof(pco),
	};

	vk::PipelineLayoutCreateInfo pl_layout_cinfo{
	    .setLayoutCount         = 1,
	    .pSetLayouts            = &desc_allocation.set_layout,
	    .pushConstantRangeCount = 1,
	    .pPushConstantRanges    = &push_constant_range,
	};

	GraphicsPipeline pl = create_graphics_pipeline(render_pass, pl_layout_cinfo, "prefilter.vert.spv", "prefilter.frag.spv");

	std::array<vk::ClearValue, 1> clear_values = {
	    std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f},
	};
	vk::RenderPassBeginInfo pass_begin_info{
	    .renderPass  = render_pass.get_handle(),
	    .framebuffer = framebuffer.get_handle(),
	    .renderArea  = {
	         .extent = {
	             .width  = cube_meta.extent.width,
	             .height = cube_meta.extent.height,
            },
        },
	    .clearValueCount = to_u32(clear_values.size()),
	    .pClearValues    = clear_values.data(),
	};

	CommandBuffer     bake_buf        = device_.begin_one_time_buf();
	vk::CommandBuffer bake_buf_handle = bake_buf.get_handle();
	vk::Viewport      viewport{
	         .x = 0,
	         .y = 0,
    };
	vk::Rect2D scissor{
	    .offset = {
	        .x = 0,
	        .y = 0,
	    },
	    .extent = {
	        .width  = cube_meta.extent.width,
	        .height = cube_meta.extent.height,
	    },
	};

	bake_buf.set_image_layout(result_.p_prefilter->resource, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer);

	uint32_t img_width  = cube_meta.extent.width;
	uint32_t img_height = cube_meta.extent.height;
	uint32_t div        = 1u;

	// See irradiance map rendering. We go through the same process here.
	for (uint32_t m = 0; m < cube_meta.levels; m++)
	{
		img_width  = std::max(1u, cube_meta.extent.width >> m);
		img_height = std::max(1u, cube_meta.extent.height >> m);
		// The higher the mipmap levels, the higher the roughness.
		// Lower resolution has less of a impact when the roughness is high.
		pco.roughness = m / static_cast<float>(cube_meta.levels - 1);

		for (uint32_t f = 0; f < 6; f++)
		{
			viewport.width  = img_width;
			viewport.height = img_height;
			pco.proj        = glm::perspective(glm::pi<float>() / 2.0f, 1.0f, 0.1f, 512.0f) * CUBE_FACE_MATRIXS[f];

			bake_buf_handle.pushConstants<PCO>(pl.get_pipeline_layout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pco);
			bake_buf_handle.setViewport(0, viewport);
			bake_buf_handle.setScissor(0, scissor);
			bake_buf_handle.beginRenderPass(pass_begin_info, vk::SubpassContents::eInline);
			bake_buf_handle.bindPipeline(vk::PipelineBindPoint::eGraphics, pl.get_handle());
			bake_buf_handle.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pl.get_pipeline_layout(), 0, desc_allocation.set, {});
			draw_box(bake_buf);
			bake_buf_handle.endRenderPass();

			vk::ImageCopy copy_region = {
			    .srcSubresource = {
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .mipLevel       = 0,
			        .baseArrayLayer = 0,
			        .layerCount     = 1,
			    },
			    .dstSubresource{
			        .aspectMask     = vk::ImageAspectFlagBits::eColor,
			        .mipLevel       = m,
			        .baseArrayLayer = f,
			        .layerCount     = 1,
			    },
			    .extent = {
			        .width  = img_width,
			        .height = img_height,
			        .depth  = 1,
			    },
			};
			transfer_from_src_to_texture(bake_buf, transfer_src_resrc, *result_.p_prefilter, copy_region);
		}
	}

	bake_buf.set_image_layout(result_.p_prefilter->resource, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	device_.end_one_time_buf(bake_buf);
}

// Create the brdf lut texture and bake it.
void PBRBaker::prepare_brdf_lut()
{
	create_brdf_lut_texture();
	if (rdoc_api)
		rdoc_api->StartFrameCapture(NULL, NULL);
	bake_brdf_lut();
	if (rdoc_api)
		rdoc_api->EndFrameCapture(NULL, NULL);
}

// The brdf lut texture is just a 2d teuxtre.
void PBRBaker::create_brdf_lut_texture()
{
	vk::ImageCreateInfo image_cinfo{
	    .imageType = vk::ImageType::e2D,
	    .format    = vk::Format::eR16G16Sfloat,
	    .extent    = {
	           .width  = BRDF_LUT_DIMENSION,
	           .height = BRDF_LUT_DIMENSION,
	           .depth  = 1,
        },
	    .mipLevels   = 1,
	    .arrayLayers = 1,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	};

	Image img = device_.get_device_memory_allocator().allocate_device_only_image(image_cinfo);

	vk::ImageViewCreateInfo view_cinfo = ImageView::two_dim_view_cinfo(img.get_handle(), vk::Format::eR16G16Sfloat, vk::ImageAspectFlagBits::eColor, 1);

	ImageView view = ImageView(device_, view_cinfo);

	vk::SamplerCreateInfo sample_cinfo = Sampler::linear_clamp_cinfo(device_.get_physical_device(), 1);

	result_.p_brdf_lut = std::make_unique<PBRTexture>(
	    ImageResource(std::move(img), std::move(view)),
	    Sampler(device_, sample_cinfo));
}

// Bake the brdf lut.
void PBRBaker::bake_brdf_lut()
{
	RenderPass                   render_pass = create_color_only_renderpass(vk::Format::eR16G16Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	Framebuffer                  framebuffer = create_square_framebuffer(render_pass, result_.p_brdf_lut->resource.get_view(), BRDF_LUT_DIMENSION);
	vk::PipelineLayoutCreateInfo pl_layout_cinfo;

	GraphicsPipelineState pl_state{
	    .vert_shader_name    = "brdf_lut.vert.spv",
	    .frag_shader_name    = "brdf_lut.frag.spv",
	    .rasterization_state = {
	        .cull_mode = vk::CullModeFlagBits::eNone,
	    },
	    .depth_stencil_state = {
	        .depth_test_enable  = false,
	        .depth_write_enable = false,
	        .depth_compare_op   = vk::CompareOp::eLessOrEqual,
	    },
	};
	GraphicsPipeline pl = GraphicsPipeline(device_, render_pass, pl_state, pl_layout_cinfo);

	vk::ClearValue clear_value = {
	    .color = {
	        std::array<float, 4>{0.54f, 0.81f, 0.94f, 1.0f},
	    },
	};

	vk::RenderPassBeginInfo pass_begin_info{
	    .renderPass  = render_pass.get_handle(),
	    .framebuffer = framebuffer.get_handle(),
	    .renderArea  = {
	         .extent = {
	             .width  = BRDF_LUT_DIMENSION,
	             .height = BRDF_LUT_DIMENSION,
            },
        },
	    .clearValueCount = 1,
	    .pClearValues    = &clear_value,
	};

	CommandBuffer     bake_buf        = device_.begin_one_time_buf();
	vk::CommandBuffer bake_buf_handle = bake_buf.get_handle();
	vk::Viewport      viewport{
	         .x        = 0,
	         .y        = 0,
	         .width    = BRDF_LUT_DIMENSION,
	         .height   = BRDF_LUT_DIMENSION,
	         .minDepth = 0,
	         .maxDepth = 1,
    };
	vk::Rect2D scissor{
	    .offset = {
	        .x = 0,
	        .y = 0,
	    },
	    .extent = {
	        .width  = BRDF_LUT_DIMENSION,
	        .height = BRDF_LUT_DIMENSION,
	    },
	};

	// We simply render directly to the brdf texture. No copying is needed.
	bake_buf_handle.beginRenderPass(pass_begin_info, vk::SubpassContents::eInline);
	bake_buf_handle.setViewport(0, viewport);
	bake_buf_handle.setScissor(0, scissor);
	bake_buf_handle.bindPipeline(vk::PipelineBindPoint::eGraphics, pl.get_handle());
	bake_buf_handle.draw(3, 1, 0, 0);
	bake_buf_handle.endRenderPass();
	device_.get_graphics_queue().waitIdle();
	device_.end_one_time_buf(bake_buf);
}

// Helper function to create a cube PBRTexture
std::unique_ptr<PBRTexture> PBRBaker::create_empty_cube_texture(ImageMetaInfo &cube_meta)
{
	vk::SamplerCreateInfo sampler_cinfo = Sampler::linear_clamp_cinfo(device_.get_physical_device(), cube_meta.levels);

	return std::make_unique<PBRTexture>(create_empty_cubic_img_resource(cube_meta), Sampler(device_, sampler_cinfo));
}

// Create a cube image resource with max mipmap levels.
ImageResource PBRBaker::create_empty_cubic_img_resource(ImageMetaInfo &meta)
{
	vk::ImageCreateInfo img_cinfo{
	    .flags       = vk::ImageCreateFlagBits::eCubeCompatible,
	    .imageType   = vk::ImageType::e2D,
	    .format      = meta.format,
	    .extent      = meta.extent,
	    .mipLevels   = meta.levels,
	    .arrayLayers = 6,
	    .samples     = vk::SampleCountFlagBits::e1,
	    .tiling      = vk::ImageTiling::eOptimal,
	    .usage       = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
	    .sharingMode = vk::SharingMode::eExclusive,
	};

	Image img = device_.get_device_memory_allocator().allocate_device_only_image(img_cinfo);

	vk::ImageViewCreateInfo view_cinfo = ImageView::cube_view_cinfo(img.get_handle(), meta.format, vk::ImageAspectFlagBits::eColor, meta.levels);

	return ImageResource(std::move(img), ImageView(device_, view_cinfo));
}

// Create a renderpass that targets one color attachment.
RenderPass PBRBaker::create_color_only_renderpass(vk::Format format, vk::ImageLayout initial_layout, vk::ImageLayout final_layout)
{
	vk::AttachmentDescription color_attachment = RenderPass::color_attachment(format, initial_layout, final_layout);
	vk::AttachmentReference   color_ref{
	      .attachment = 0,
	      .layout     = vk::ImageLayout::eColorAttachmentOptimal,
    };

	vk::SubpassDescription subpass_description{
	    .pipelineBindPoint    = vk::PipelineBindPoint::eGraphics,
	    .colorAttachmentCount = 1,
	    .pColorAttachments    = &color_ref,
	};

	// Default subpass dependency for the beginning of the renderpass and the end of the renderpass.
	std::array<vk::SubpassDependency, 2> dependencys;
	dependencys[0] = vk::SubpassDependency{
	    .srcSubpass      = VK_SUBPASS_EXTERNAL,
	    .dstSubpass      = 0,
	    .srcStageMask    = vk::PipelineStageFlagBits::eBottomOfPipe,
	    .dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput,
	    .srcAccessMask   = vk::AccessFlagBits::eMemoryRead,
	    .dstAccessMask   = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
	    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
	};
	dependencys[1] = vk::SubpassDependency{
	    .srcSubpass      = 0,
	    .dstSubpass      = VK_SUBPASS_EXTERNAL,
	    .srcStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput,
	    .dstStageMask    = vk::PipelineStageFlagBits::eBottomOfPipe,
	    .srcAccessMask   = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
	    .dstAccessMask   = vk::AccessFlagBits::eMemoryRead,
	    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
	};

	vk::RenderPassCreateInfo render_pass_cinfo{
	    .attachmentCount = 1,
	    .pAttachments    = &color_attachment,
	    .subpassCount    = 1,
	    .pSubpasses      = &subpass_description,
	    .dependencyCount = 2,
	    .pDependencies   = dependencys.data(),
	};

	return RenderPass(device_, render_pass_cinfo);
}

// Create an image resource suitable for being the transfer resource.
// The result will be used as a framebuffer in bake_irradiance and bake_prefilter.
ImageResource PBRBaker::create_transfer_src(vk::Extent3D extent, vk::Format format)
{
	vk::ImageCreateInfo transfer_src_cinfo{
	    .imageType     = vk::ImageType::e2D,
	    .format        = format,
	    .extent        = extent,
	    .mipLevels     = 1,
	    .arrayLayers   = 1,
	    .samples       = vk::SampleCountFlagBits::e1,
	    .tiling        = vk::ImageTiling::eOptimal,
	    .usage         = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
	    .sharingMode   = vk::SharingMode::eExclusive,
	    .initialLayout = vk::ImageLayout::eUndefined,
	};

	Image img = device_.get_device_memory_allocator().allocate_device_only_image(transfer_src_cinfo);

	vk::ImageViewCreateInfo view_cinfo = ImageView::two_dim_view_cinfo(img.get_handle(), format, vk::ImageAspectFlagBits::eColor, 1);

	ImageResource resrc = ImageResource(std::move(img), ImageView(device_, view_cinfo));

	CommandBuffer cmd_buf = device_.begin_one_time_buf();
	cmd_buf.set_image_layout(resrc, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
	device_.end_one_time_buf(cmd_buf);

	return resrc;
}

// Helper function to create a square framebuffer.
Framebuffer PBRBaker::create_square_framebuffer(const RenderPass &render_pass, const ImageView &view, uint32_t dimension)
{
	vk::ImageView             view_handle = view.get_handle();
	vk::FramebufferCreateInfo framebuffer_cinfo{
	    .renderPass      = render_pass.get_handle(),
	    .attachmentCount = 1,
	    .pAttachments    = &view_handle,
	    .width           = dimension,
	    .height          = dimension,
	    .layers          = 1,
	};
	return Framebuffer(device_, framebuffer_cinfo);
}

// Helper function to create a graphics pipeline with the default state.
GraphicsPipeline PBRBaker::create_graphics_pipeline(RenderPass &render_pass, vk::PipelineLayoutCreateInfo &pl_layout_cinfo, const char *vert_shader_name, const char *frag_shader_name)
{
	std::array<vk::VertexInputBindingDescription, 1> binding_descriptions;
	binding_descriptions[0] = vk::VertexInputBindingDescription{
	    .binding   = 0,
	    .stride    = sizeof(sg::Vertex),
	    .inputRate = vk::VertexInputRate::eVertex,
	};

	GraphicsPipelineState state{
	    .vert_shader_name   = vert_shader_name,
	    .frag_shader_name   = frag_shader_name,
	    .vertex_input_state = {
	        .attribute_descriptions = sg::Vertex::get_input_attr_descriptions(),
	        .binding_descriptions   = binding_descriptions,
	    },
	    .depth_stencil_state = {
	        .depth_test_enable  = false,
	        .depth_write_enable = false,
	        .depth_compare_op   = vk::CompareOp::eLessOrEqual,
	    },
	};
	return GraphicsPipeline(device_, render_pass, state, pl_layout_cinfo);
}

// Helper function to create descriptors for the texture.
DescriptorAllocation PBRBaker::allocate_texture_descriptor(PBRTexture &texture)
{
	vk::DescriptorImageInfo desc_iinfo{
	    .sampler     = texture.sampler.get_handle(),
	    .imageView   = texture.resource.get_view().get_handle(),
	    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	};
	return DescriptorBuilder::begin(desc_state_.cache, desc_state_.allocator).bind_image(0, desc_iinfo, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment).build();
}

// Helper function that writes the commands to draw the textured box.
void PBRBaker::draw_box(CommandBuffer &cmd_buf)
{
	vk::CommandBuffer cmd_buf_handle = cmd_buf.get_handle();
	cmd_buf_handle.bindVertexBuffers(0, result_.p_box->p_vertex_buf_->get_handle(), {0});
	cmd_buf_handle.bindIndexBuffer(result_.p_box->p_idx_buf_->get_handle(), 0, vk::IndexType::eUint32);
	cmd_buf_handle.drawIndexed(result_.p_box->idx_count_, 1, 0, 0, 0);
}

// Helper function that copy data from the src to the PBRTexture.
void PBRBaker::transfer_from_src_to_texture(CommandBuffer &cmd_buf, ImageResource &src, PBRTexture &texture, vk::ImageCopy copy_region)
{
	cmd_buf.set_image_layout(src, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
	cmd_buf.get_handle().copyImage(src.get_image().get_handle(), vk::ImageLayout::eTransferSrcOptimal, texture.resource.get_image().get_handle(), vk::ImageLayout::eTransferDstOptimal, copy_region);
	cmd_buf.set_image_layout(src, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eColorAttachmentOptimal);
}

}        // namespace W3D