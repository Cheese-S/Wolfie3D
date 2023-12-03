#include "sync_objects.hpp"

#include "device.hpp"

namespace W3D
{

// Create a fence with given flags
Fence::Fence(Device &device, vk::FenceCreateFlags flags) :
    device_(device)
{
	vk::FenceCreateInfo fence_cinfo{
	    .flags = flags,
	};
	handle_ = device_.get_handle().createFence(fence_cinfo);
}

// Move constructor for Fence
Fence::Fence(Fence &&rhs) :
    VulkanObject(std::move(rhs)),
    device_(rhs.device_){};

// Fence clean up
Fence::~Fence()
{
	device_.get_handle().destroyFence(handle_);
}

// Create a default semaphore
Semaphore::Semaphore(Device &device) :
    device_(device)
{
	vk::SemaphoreCreateInfo semaphore_cinfo{};
	handle_ = device_.get_handle().createSemaphore(semaphore_cinfo);
}

// Move constructor for Semaphore
Semaphore::Semaphore(Semaphore &&rhs) :
    VulkanObject(std::move(rhs)),
    device_(rhs.device_)
{
}

// Semaphore clean up
Semaphore::~Semaphore()
{
	device_.get_handle().destroySemaphore(handle_);
}

}        // namespace W3D