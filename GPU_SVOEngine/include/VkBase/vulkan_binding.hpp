#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>


class VulkanBinding
{
public:
	VulkanBinding(uint32_t binding, VkVertexInputRate rate, uint32_t stride);

	void addAttribDescription(VkFormat format, uint32_t offset);

	[[nodiscard]] uint32_t getStride() const;

private:
	struct AttributeData
	{
		uint32_t location;
		VkFormat format;
		uint32_t offset;

		AttributeData(uint32_t location, VkFormat format, uint32_t offset);

		[[nodiscard]] VkVertexInputAttributeDescription getAttributeDescription(uint32_t p_binding) const;
	};

	uint32_t m_binding;
	VkVertexInputRate m_rate;
	uint32_t m_stride;
	std::vector<AttributeData> m_attributes;

	[[nodiscard]] VkVertexInputBindingDescription getBindingDescription() const;
	[[nodiscard]] std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;

	friend class VulkanPipeline;
	friend struct VulkanPipelineBuilder;
};
