#pragma once

#include "common/bit_flags.hpp"
#include "common/glm_common.hpp"
#include "common/vk_common.hpp"
#include "scene_graph/components/material.hpp"

namespace W3D::sg
{

enum class PBRMaterialFlagBits
{
	eBaseColorTexture         = 1 << 0,
	eNormalTexture            = 1 << 1,
	eOcclusionTexture         = 1 << 2,
	eEmissiveTexture          = 1 << 3,
	eMetallicRoughnessTexture = 1 << 4,
	eVertexColor              = 1 << 5,
};

using PBRMaterialFlag = BitFlags<PBRMaterialFlagBits>;

// PBRMaterial component.
// It contains a descriptor set that binds all its textures.
class PBRMaterial : public Material
{
  public:
	PBRMaterial(const std::string &name);
	virtual ~PBRMaterial() = default;
	virtual std::type_index get_type() override;
	glm::vec4               base_color_factor_{0.0f, 0.0f, 0.0f, 0.0f};
	float                   metallic_factor_{0.0f};
	float                   roughness_factor_{0.0f};
	PBRMaterialFlag         flag_;        // Used to tell shaders which textures are set.
	vk::DescriptorSet       set_;
};
}        // namespace W3D::sg