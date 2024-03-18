#include "VkBase/vulkan_descriptors.hpp"

#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"

VkDescriptorPool VulkanDescriptorPool::operator*() const
{
	return m_vkHandle;
}

void VulkanDescriptorPool::free()
{
	if (m_vkHandle != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(VulkanContext::getDevice(m_device).m_vkHandle, m_vkHandle, nullptr);
		m_vkHandle = VK_NULL_HANDLE;
	}
}

VulkanDescriptorPool::VulkanDescriptorPool(const uint32_t device, const VkDescriptorPool descriptorPool)
	: m_vkHandle(descriptorPool), m_device(device)
{

}
