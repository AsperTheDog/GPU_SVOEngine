#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>

#include "vulkan_device.hpp"
#include "vulkan_queues.hpp"

class VulkanGPU;

class VulkanContext : public VulkanBase
{
public:
	static void init(uint32_t vulkanApiVersion, bool enableValidationLayers, const std::vector<const char*>& extensions);

	static [[nodiscard]] std::vector<VulkanGPU> getGPUs();

	static uint32_t createDevice(VulkanGPU gpu, const QueueFamilySelector& queues, const std::vector<const char*>& extensions, const VkPhysicalDeviceFeatures& features);
	static VulkanDevice& getDevice(uint32_t index);
	static void freeDevice(uint32_t index);
	static void freeDevice(const VulkanDevice& device);

	static void free();

private:
	inline static VkInstance m_vkHandle = VK_NULL_HANDLE;
	inline static bool m_validationLayersEnabled = false;

	inline static std::vector<VulkanDevice> m_devices{};

	friend class SDLWindow;
};

