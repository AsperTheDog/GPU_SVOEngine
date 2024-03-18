#include "VkBase/vulkan_context.hpp"

#include <stdexcept>
#include <vector>

#include "VkBase/vulkan_device.hpp"
#include "VkBase/vulkan_gpu.hpp"
#include "VkBase/vulkan_queues.hpp"

std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

void VulkanContext::init(const uint32_t vulkanApiVersion, const bool enableValidationLayers, const std::vector<const char*>& extensions)
{
	m_validationLayersEnabled = enableValidationLayers;

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Application";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = vulkanApiVersion;

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enableValidationLayers)
	{
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		instanceCreateInfo.enabledLayerCount = 0;
	}
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_vkHandle) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}
}

std::vector<VulkanGPU> VulkanContext::getGPUs()
{
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(m_vkHandle, &gpuCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	vkEnumeratePhysicalDevices(m_vkHandle, &gpuCount, physicalDevices.data());

	std::vector<VulkanGPU> gpus;
	gpus.reserve(physicalDevices.size());
	for (const auto& physicalDevice : physicalDevices)
	{
		gpus.push_back(VulkanGPU(physicalDevice));
	}
	return gpus;
}

uint32_t VulkanContext::createDevice(const VulkanGPU gpu, const QueueFamilySelector& queues, const std::vector<const char*>& extensions, const VkPhysicalDeviceFeatures& features)
{
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	if (m_validationLayersEnabled)
	{
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		deviceCreateInfo.enabledLayerCount = 0;
	}
	if (!extensions.empty())
	{
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = extensions.data();
	}
	else
	{
		deviceCreateInfo.enabledExtensionCount = 0;
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	for (const uint32_t index : queues.getUniqueIndices())
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = index;
		queueCreateInfo.queueCount = static_cast<uint32_t>(queues.m_selections[index].priorities.size());
		queueCreateInfo.pQueuePriorities = queues.m_selections[index].priorities.data();
		if (queues.m_selections[index].familyFlags & QueueFamilyTypeBits::PROTECTED)
		{
			queueCreateInfo.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
		}
		queueCreateInfos.push_back(queueCreateInfo);
	}
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

	deviceCreateInfo.pEnabledFeatures = &features;

	VkDevice device;
	if (vkCreateDevice(gpu.m_vkHandle, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	m_devices.push_back({gpu, device});
	return m_devices.back().getID();
}

VulkanDevice& VulkanContext::getDevice(const uint32_t index)
{
	for (auto& device : m_devices)
	{
		if (device.getID() == index)
		{
			return device;
		}
	}

	throw std::runtime_error("Device not found");
}

void VulkanContext::freeDevice(const uint32_t index)
{
	for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
	{
		if (it->getID() == index)
		{
			m_devices.erase(it);
			break;
		}
	}
}

void VulkanContext::freeDevice(const VulkanDevice& device)
{
	freeDevice(device.getID());
}

void VulkanContext::free()
{
	for (auto& device : m_devices)
	{
		device.free();
	}
	m_devices.clear();

	vkDestroyInstance(m_vkHandle, nullptr);
	m_vkHandle = VK_NULL_HANDLE;
}

VkInstance VulkanContext::getHandle()
{
	return m_vkHandle;
}
