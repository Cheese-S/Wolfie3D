#include "sync_objects.hpp"

#include "device.hpp"

namespace W3D
{
Fence::Fence(Device &device, vk::FenceCreateFlags flags) :
    device_(device)
{
	vk::FenceCreateInfo fence_cinfo{
	    .flags = flags,
	};
	handle_ = device_.get_handle().createFence(fence_cinfo);
}

Fence::Fence(Fence &&rhs) :
    VulkanObject(std::move(rhs)),
    device_(rhs.device_){};

Fence::~Fence()
{
	if (handle_)
	{
		device_.get_handle().destroyFence(handle_);
	}
}

Semaphore::Semaphore(Device &device) :
    device_(device)
{
	vk::SemaphoreCreateInfo semaphore_cinfo{};
	handle_ = device_.get_handle().createSemaphore(semaphore_cinfo);
}

Semaphore::Semaphore(Semaphore &&rhs) :
    VulkanObject(std::move(rhs)),
    device_(rhs.device_)
{
}

Semaphore::~Semaphore()
{
	if (handle_)
	{
		device_.get_handle().destroySemaphore(handle_);
	}
}

}        // namespace W3D