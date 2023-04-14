#pragma once
#include <vector>

#include "common.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"


namespace W3D::vk_utils {
vk::Format findSupportedFormat(const std::vector<VkFormat>& candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlagBits features);
};