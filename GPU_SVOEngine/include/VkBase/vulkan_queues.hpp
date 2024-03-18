#pragma once
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "vulkan_gpu.hpp"

class QueueFamily;

enum QueueFamilyTypeBits
{
	GRAPHICS = 1,
	COMPUTE = 2,
	PRESENT = 4,
	SPARSE_BINDING = 8,
	VIDEO_DECODE = 16,
	OPTICAL_FLOW = 32,
	PROTECTED = 64
};
typedef uint8_t QueueFamilyTypes;

class GPUQueueStructure
{
public:
	[[nodiscard]] uint32_t getQueueFamilyCount() const;
	[[nodiscard]] QueueFamily getQueueFamily(uint32_t index) const;
	[[nodiscard]] QueueFamily findQueueFamily(VkQueueFlags flags, bool exactMatch = false) const;
	[[nodiscard]] QueueFamily findPresentQueueFamily(VkSurfaceKHR surface) const;

	[[nodiscard]] std::string toString() const;

private:
	GPUQueueStructure() = default;
	explicit GPUQueueStructure(VulkanGPU gpu);

	std::vector<QueueFamily> queueFamilies;
	VulkanGPU gpu;

	friend class VulkanGPU;
	friend class QueueFamilySelector;
};

class QueueFamily
{
public:
	VkQueueFamilyProperties properties;
	uint32_t index;
	VulkanGPU gpu;

private:
	QueueFamily(const VkQueueFamilyProperties& properties, uint32_t index, VulkanGPU gpu);

	friend class GPUQueueStructure;
};

class VulkanQueue
{
public:
	void waitIdle() const;

	VkQueue operator*() const;

private:
	explicit VulkanQueue(VkQueue queue);

	VkQueue m_vkHandle;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
	friend class SDLWindow;
};

struct QueueSelection
{
	uint32_t familyIndex = UINT32_MAX;
	uint32_t queueIndex = UINT32_MAX;
};

class QueueFamilySelector
{
public:
	explicit QueueFamilySelector(const GPUQueueStructure& structure);

	void selectQueueFamily(const QueueFamily& family, QueueFamilyTypes typeMask);
	QueueSelection getOrAddQueue(const QueueFamily& family, float priority);
	QueueSelection addQueue(const QueueFamily& family, float priority);

	[[nodiscard]] std::optional<QueueFamily> getQueueFamilyByType(QueueFamilyTypes type);
	[[nodiscard]] std::vector<uint32_t> getUniqueIndices() const;

private:
	struct QueueSelections
	{
		QueueFamilyTypes familyFlags;
		std::vector<float> priorities;
	};
	GPUQueueStructure m_structure;

	std::vector<QueueSelections> m_selections;

	friend class GPUQueueStructure;
	friend class VulkanContext;
};