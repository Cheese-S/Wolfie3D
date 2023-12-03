#pragma once

#include "common/vk_common.hpp"
#include "core/vulkan_object.hpp"

namespace W3D
{

class Device;
class CommandBuffer;

// We either reset the pool (resetting all the buffers allocated from this pool together) or reset individual buffers.
enum class CommandPoolResetStrategy
{
	eIndividual,
	ePool
};

// RAII wrapper for vkCommandPool
class CommandPool : public VulkanObject<vk::CommandPool>
{
  public:
	CommandPool(Device &device, const vk::Queue &queue, uint32_t queue_family_index, CommandPoolResetStrategy strategy = CommandPoolResetStrategy::eIndividual, vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	~CommandPool() override;

	CommandBuffer              allocate_command_buffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
	std::vector<CommandBuffer> allocate_command_buffers(uint32_t count, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
	void                       free_command_buffers(std::vector<CommandBuffer> &cmd_bufs);
	void                       free_command_buffer(CommandBuffer &cmd_buf);
	void                       recycle_command_buffer(CommandBuffer &cmd_buf);

	void                     reset();
	CommandPoolResetStrategy get_reset_strategy();
	const vk::Queue         &get_queue();
	const Device            &get_device();

  private:
	Device                    &device_;
	const vk::Queue           &queue_;
	std::vector<CommandBuffer> primary_cmd_bufs_;
	std::vector<CommandBuffer> secondary_cmd_bufs_;
	CommandPoolResetStrategy   strategy_;
};

}        // namespace W3D