#include "VkBase/vulkan_binding.hpp"

VulkanBinding::VulkanBinding(const uint32_t binding, const VkVertexInputRate rate, const uint32_t stride)
	: m_binding(binding), m_rate(rate), m_stride(stride)
{
	
}

uint32_t VulkanBinding::getStride() const
{
	return m_stride;
}

void VulkanBinding::addAttribDescription(VkFormat format, uint32_t offset)
{
	m_attributes.emplace_back(static_cast<uint32_t>(m_attributes.size()), format, offset);
}

VulkanBinding::AttributeData::AttributeData(const uint32_t location, const VkFormat format, const uint32_t offset)
	: location(location), format(format), offset(offset)
{
}

VkVertexInputAttributeDescription VulkanBinding::AttributeData::getAttributeDescription(const uint32_t p_binding) const
{
	VkVertexInputAttributeDescription attributeDescription;
	attributeDescription.binding = p_binding;
	attributeDescription.location = location;
	attributeDescription.format = format;
	attributeDescription.offset = offset;
	return attributeDescription;
}

VkVertexInputBindingDescription VulkanBinding::getBindingDescription() const
{
	VkVertexInputBindingDescription bindingDescription;
	bindingDescription.binding = m_binding;
	bindingDescription.stride = m_stride;
	bindingDescription.inputRate = m_rate;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> VulkanBinding::getAttributeDescriptions() const
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	attributeDescriptions.reserve(m_attributes.size());
	for (auto& attr : m_attributes)
	{
		attributeDescriptions.push_back(attr.getAttributeDescription(m_binding));
	}
	return attributeDescriptions;
}