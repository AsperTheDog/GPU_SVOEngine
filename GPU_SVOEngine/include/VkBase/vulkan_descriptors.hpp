#pragma once
#include <vulkan/vulkan_core.h>

#include "vulkan_base.hpp"


class VulkanDevice;

class VulkanDescriptorPool : public VulkanBase
{
public:
	[[nodiscard]] VkDescriptorPool operator*() const;

private:
	void free();

	VulkanDescriptorPool(uint32_t device, VkDescriptorPool descriptorPool);

	VkDescriptorPool m_vkHandle = VK_NULL_HANDLE;

	uint32_t m_device = UINT32_MAX;

	friend class VulkanDevice;
};

