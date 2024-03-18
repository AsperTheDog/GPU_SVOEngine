#include "VkBase/vulkan_sync.hpp"

#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"

void VulkanFence::reset()
{
	vkResetFences(VulkanContext::getDevice(m_device).m_vkHandle, 1, &m_vkHandle);
	m_isSignaled = false;
}

void VulkanFence::wait()
{
	vkWaitForFences(VulkanContext::getDevice(m_device).m_vkHandle, 1, &m_vkHandle, VK_TRUE, UINT64_MAX);
	m_isSignaled = true;
}

void VulkanFence::free()
{
	vkDestroyFence(VulkanContext::getDevice(m_device).m_vkHandle, m_vkHandle, nullptr);
	m_vkHandle = VK_NULL_HANDLE;
}

bool VulkanFence::isSignaled() const
{
	return m_isSignaled;
}

VulkanFence::VulkanFence(const uint32_t device, const VkFence fence, const bool isSignaled)
	: m_vkHandle(fence), m_isSignaled(isSignaled), m_device(device)
{
}

void VulkanSemaphore::free()
{
	if (m_vkHandle != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(VulkanContext::getDevice(m_device).m_vkHandle, m_vkHandle, nullptr);
		m_vkHandle = VK_NULL_HANDLE;
	}
}

VulkanSemaphore::VulkanSemaphore(const uint32_t device, const VkSemaphore semaphore)
	: m_vkHandle(semaphore), m_device(device)
{
}
