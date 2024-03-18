#pragma once
#include <vulkan/vulkan_core.h>

#include "vulkan_base.hpp"

class VulkanDevice;

class VulkanFence : public VulkanBase
{
public:

	void reset();
	void wait();

	[[nodiscard]] bool isSignaled() const;

private:
	void free();

	VulkanFence(uint32_t device, VkFence fence, bool isSignaled);

	VkFence m_vkHandle = VK_NULL_HANDLE;

	bool m_isSignaled = false;

	uint32_t m_device;

	friend class VulkanDevice;
	friend class SDLWindow;
	friend class VulkanCommandBuffer;
};

class VulkanSemaphore : public VulkanBase
{
public:

private:
	void free();

	VulkanSemaphore(uint32_t device, VkSemaphore semaphore);

	VkSemaphore m_vkHandle = VK_NULL_HANDLE;

	uint32_t m_device;

	friend class VulkanDevice;
	friend class SDLWindow;
	friend class VulkanCommandBuffer;
};