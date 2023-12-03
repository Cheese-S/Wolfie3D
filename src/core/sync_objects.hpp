#pragma once

#include "common/vk_common.hpp"
#include "core/vulkan_object.hpp"

namespace W3D
{
class Device;

// RAII Wrapper for VkFence
// Fence is used to inject dependency from a queue to host (GPU to CPU).
// Fence is signaled when GPU finished some work and CPU can wait on it using vkWaitFences
class Fence : public VulkanObject<vk::Fence>
{
  public:
	Fence(Device &device, vk::FenceCreateFlags flags);
	Fence(Fence &&);
	~Fence() override;

  private:
	Device &device_;
};

// RAII Wrapper for VkSemaphore
// Semaphore is used to inject dependency from a queue to a queue. (GPU to GPU).
// Semaphore is signaled when a queue finished some work nd another queue can wait on it using vkWaitSemaphore
class Semaphore : public VulkanObject<vk::Semaphore>
{
  public:
	Semaphore(Device &device);
	Semaphore(Semaphore &&);
	~Semaphore() override;

  private:
	Device &device_;
};

}        // namespace W3D