#pragma once
#include <vulkan/vulkan_core.h>

#include "vulkan_memory.hpp"
#include "vulkan_base.hpp"

class VulkanDevice;

class VulkanBuffer : public VulkanBase
{
public:
	[[nodiscard]] VkMemoryRequirements getMemoryRequirements() const;
	
	void allocateFromIndex(uint32_t memoryIndex);
	void allocateFromFlags(VulkanMemoryAllocator::MemoryPropertyPreferences memoryProperties);

	void* map(VkDeviceSize size, VkDeviceSize offset);
	void unmap();

	[[nodiscard]] bool isMemoryMapped() const;
	[[nodiscard]] void* getMappedData() const;
	[[nodiscard]] VkDeviceSize getSize() const;

private:
	void free();

	VulkanBuffer(uint32_t device, VkBuffer vkHandle, VkDeviceSize size);

	void setBoundMemory(const MemoryChunk::MemoryBlock& memoryRegion);

	VkBuffer m_vkHandle = VK_NULL_HANDLE;

	MemoryChunk::MemoryBlock m_memoryRegion;
	VkDeviceSize m_size = 0;
	void* m_mappedData = nullptr;

	uint32_t m_device;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};

