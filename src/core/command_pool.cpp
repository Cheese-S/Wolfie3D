#include "command_pool.hpp"

#include "command_buffer.hpp"
#include "common/utils.hpp"
#include "device.hpp"

namespace W3D
{

// Create a command pool associated with a certain family.
// * All buffers allocated from this pool will be submited to that queue.
CommandPool::CommandPool(Device &device, const vk::Queue &queue, uint32_t queue_family_index, CommandPoolResetStrategy strategy, vk::CommandPoolCreateFlags flags) :
    device_(device),
    queue_(queue),
    strategy_(strategy)

{
	vk::CommandPoolCreateInfo pool_cinfo{
	    .flags            = flags,
	    .queueFamilyIndex = queue_family_index,
	};
	handle_ = device_.get_handle().createCommandPool(pool_cinfo);

	vk::CommandBufferAllocateInfo cmd_buf_ainfo{};
}

CommandPool::~CommandPool()
{
	if (handle_)
	{
		// Some driver implementations fails to recollect memory from the buffers before freeing the buffers.
		// Therefore, we must call reset before destroying the pool.
		reset();
		device_.get_handle().destroyCommandPool(handle_);
		handle_ = nullptr;
	}
}

// Helper function to allocate one single buffer.
CommandBuffer CommandPool::allocate_command_buffer(vk::CommandBufferLevel level)
{
	return std::move(allocate_command_buffers(1, level)[0]);
}

// Allocate command buffers. Grab buffers from free list first. If there aren't enough buffers, allocate new ones.
std::vector<CommandBuffer> CommandPool::allocate_command_buffers(uint32_t count, vk::CommandBufferLevel level)
{
	std::vector<CommandBuffer> &free_list = level == vk::CommandBufferLevel::ePrimary ? primary_cmd_bufs_ : secondary_cmd_bufs_;
	std::vector<CommandBuffer>  cmd_bufs;
	cmd_bufs.reserve(count);

	while (count && free_list.size())
	{
		cmd_bufs.emplace_back(std::move(free_list.back()));
		free_list.pop_back();
		count--;
	}

	if (!count)
	{
		return cmd_bufs;
	}

	vk::CommandBufferAllocateInfo cmd_buf_ainfo{
	    .commandPool        = handle_,
	    .level              = level,
	    .commandBufferCount = to_u32(count),
	};
	std::vector<vk::CommandBuffer> cmd_buf_handles = device_.get_handle().allocateCommandBuffers(cmd_buf_ainfo);

	for (auto handle : cmd_buf_handles)
	{
		cmd_bufs.emplace_back(CommandBuffer(handle, *this, level));
	}
	return cmd_bufs;
}

// Recycle buffers and push them to free list.
void CommandPool::recycle_command_buffer(CommandBuffer &cmd_buf)
{
	if (cmd_buf.level_ == vk::CommandBufferLevel::ePrimary)
	{
		primary_cmd_bufs_.push_back(std::move(cmd_buf));
	}
	else
	{
		secondary_cmd_bufs_.push_back(std::move(cmd_buf));
	}
}

// Helper function to free an array of buffers
void CommandPool::free_command_buffers(std::vector<CommandBuffer> &cmd_bufs)
{
	std::vector<vk::CommandBuffer> cmd_buf_handles;
	cmd_buf_handles.reserve(cmd_bufs.size());
	for (auto &cmd_buf : cmd_bufs)
	{
		cmd_buf_handles.push_back(cmd_buf.get_handle());
		cmd_buf.handle_ = nullptr;
	};
	device_.get_handle().freeCommandBuffers(handle_, cmd_buf_handles);
}

// Helper function that frees one cmd buf.
void CommandPool::free_command_buffer(CommandBuffer &cmd_buf)
{
	device_.get_handle().freeCommandBuffers(handle_, cmd_buf.get_handle());
	cmd_buf.handle_ = nullptr;
}

// Reset the command pool.
void CommandPool::reset()
{
	device_.get_handle().resetCommandPool(handle_);
}

CommandPoolResetStrategy CommandPool::get_reset_strategy()
{
	return strategy_;
}

const vk::Queue &CommandPool::get_queue()
{
	return queue_;
}
}        // namespace W3D