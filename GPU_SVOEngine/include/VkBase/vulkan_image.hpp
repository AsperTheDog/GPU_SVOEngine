#pragma once
#include <vulkan/vulkan_core.h>

#include "vulkan_base.hpp"
#include "vulkan_memory.hpp"

class VulkanDevice;

class VulkanImage : public VulkanBase
{
public:
	[[nodiscard]] VkMemoryRequirements getMemoryRequirements() const;
	
	void allocateFromIndex(uint32_t memoryIndex);
	void allocateFromFlags(VulkanMemoryAllocator::MemoryPropertyPreferences memoryProperties);

	VkImageView createImageView(VkFormat format, VkImageAspectFlags aspectFlags);
	void freeImageView(VkImageView imageView);

	void transitionLayout(const VkImageLayout layout, const VkImageAspectFlags aspectFlags, const uint32_t srcQueueFamily, const uint32_t
	                      dstQueueFamily, uint32_t threadID);

	[[nodiscard]] VkExtent3D getSize() const;

private:
	void free();

	VulkanImage(uint32_t device, VkImage vkHandle, VkExtent3D size, VkImageType type, VkImageLayout layout);

	void setBoundMemory(const MemoryChunk::MemoryBlock& memoryRegion);

	MemoryChunk::MemoryBlock m_memoryRegion;
	VkExtent3D m_size{};
	VkImageType m_type;
	VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	VkImage m_vkHandle = VK_NULL_HANDLE;
	uint32_t m_device;

	std::vector<VkImageView> m_imageViews;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};

