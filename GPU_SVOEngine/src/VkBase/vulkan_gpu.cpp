#include "VkBase/vulkan_gpu.hpp"

#include <stdexcept>

#include "VkBase/sdl_window.hpp"
#include "VkBase/vulkan_queues.hpp"

VkPhysicalDeviceProperties VulkanGPU::getProperties() const
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(m_vkHandle, &properties);
	return properties;
}

VkPhysicalDeviceFeatures VulkanGPU::getFeatures() const
{
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(m_vkHandle, &features);
	return features;
}

VkPhysicalDeviceMemoryProperties VulkanGPU::getMemoryProperties() const
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkHandle, &memoryProperties);
	return memoryProperties;
}

VkSurfaceCapabilitiesKHR VulkanGPU::getCapabilities(const SDLWindow& window) const
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkHandle, window.m_surface, &capabilities);
	return capabilities;
}

GPUQueueStructure VulkanGPU::getQueueFamilies() const
{
	return GPUQueueStructure(*this);
}

bool VulkanGPU::isFormatSupported(const VkSurfaceKHR surface, const VkSurfaceFormatKHR format) const
{
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkHandle, surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkHandle, surface, &formatCount, formats.data());
	for (const VkSurfaceFormatKHR& availableFormat : formats)
	{
		if (availableFormat.colorSpace == format.colorSpace && availableFormat.format == format.format)
		{
			return true;
		}
	}
	return false;
}

VkSurfaceFormatKHR VulkanGPU::getClosestFormat(const SDLWindow& window, const VkSurfaceFormatKHR format) const
{
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkHandle, window.m_surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkHandle, window.m_surface, &formatCount, formats.data());
	std::optional<VkSurfaceFormatKHR> formatMatch = std::nullopt;
	std::optional<VkSurfaceFormatKHR> colorMatch = std::nullopt;
	for (const VkSurfaceFormatKHR& availableFormat : formats)
	{
		if (availableFormat.colorSpace == format.colorSpace && availableFormat.format == format.format)
			return availableFormat;
		
		if (availableFormat.format == format.format && !formatMatch.has_value())
			formatMatch = availableFormat;
		
		else if (availableFormat.colorSpace == format.colorSpace && !colorMatch.has_value())
			colorMatch = availableFormat;
	}
	if (formatMatch.has_value())
		return formatMatch.value();
	
	if (colorMatch.has_value())
		return colorMatch.value();

	return formats[0];
}

VkFormatProperties VulkanGPU::getFormatProperties(const VkFormat format) const
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_vkHandle, format, &formatProperties);
	return formatProperties;
}

VkFormat VulkanGPU::findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling, const VkFormatFeatureFlags features) const
{
	for (const VkFormat format : candidates)
	{
		const VkFormatProperties props = getFormatProperties(format);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
		if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
	}

	throw std::runtime_error("Failed to find supported format");
}

VkPhysicalDevice VulkanGPU::operator*() const
{
	return m_vkHandle;
}

VulkanGPU::VulkanGPU(const VkPhysicalDevice physicalDevice)
	: m_vkHandle(physicalDevice)
{

}
