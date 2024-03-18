#pragma once
#include "vulkan_base.hpp"
#include <vulkan/vulkan_core.h>

class VulkanDevice;

class VulkanFramebuffer : public VulkanBase
{
public:

private:
	void free();

	VulkanFramebuffer(uint32_t device, VkFramebuffer handle);

	VkFramebuffer m_vkHandle = VK_NULL_HANDLE;

	uint32_t m_device;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};

