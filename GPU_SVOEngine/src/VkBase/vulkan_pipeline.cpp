#include "VkBase/vulkan_pipeline.hpp"

#include <array>

#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"
#include "VkBase/vulkan_shader.hpp"


VulkanPipelineBuilder::VulkanPipelineBuilder(VulkanDevice* device)
	: m_device(device)
{
	m_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_vertexInputState.vertexBindingDescriptionCount = 0;
	m_vertexInputState.vertexAttributeDescriptionCount = 0;

	m_inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	m_tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	m_tessellationState.patchControlPoints = 1;
	m_tesellationStateEnabled = false;

	m_viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	m_viewportState.viewportCount = 1;
	m_viewportState.scissorCount = 1;

	m_rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_rasterizationState.depthClampEnable = VK_FALSE;
	m_rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	m_rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	m_rasterizationState.lineWidth = 1.0f;
	m_rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	m_rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_rasterizationState.depthBiasEnable = VK_FALSE;

	m_multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	m_multisampleState.sampleShadingEnable = VK_FALSE;
	m_multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	m_depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	m_depthStencilState.depthTestEnable = VK_TRUE;
	m_depthStencilState.depthWriteEnable = VK_TRUE;
	m_depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	m_depthStencilState.depthBoundsTestEnable = VK_FALSE;
	m_depthStencilState.stencilTestEnable = VK_FALSE;

	m_colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_colorBlendState.logicOpEnable = VK_FALSE;
	m_colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	m_colorBlendState.attachmentCount = 0;
	m_colorBlendState.blendConstants[0] = 0.0f;
	m_colorBlendState.blendConstants[1] = 0.0f;
	m_colorBlendState.blendConstants[2] = 0.0f;
	m_colorBlendState.blendConstants[3] = 0.0f;

	m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_dynamicState.dynamicStateCount = 0;
}

void VulkanPipelineBuilder::addVertexBinding(const VulkanBinding& binding)
{
	m_vertexInputBindings.push_back(binding.getBindingDescription());
	for (auto& attr : binding.getAttributeDescriptions())
	{
		m_vertexInputAttributes.push_back(attr);
	}
	m_vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindings.size());
	m_vertexInputState.pVertexBindingDescriptions = m_vertexInputBindings.data();
	m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributes.size());
	m_vertexInputState.pVertexAttributeDescriptions = m_vertexInputAttributes.data();
}

void VulkanPipelineBuilder::setInputAssemblyState(const VkPrimitiveTopology topology, const VkBool32 primitiveRestartEnable)
{
	m_inputAssemblyState.topology = topology;
	m_inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
}

void VulkanPipelineBuilder::setTessellationState(const uint32_t patchControlPoints)
{
	m_tessellationState.patchControlPoints = patchControlPoints;
	m_tesellationStateEnabled = true;
}

void VulkanPipelineBuilder::setViewportState(const uint32_t viewportCount, const uint32_t scissorCount)
{
	m_viewportState.viewportCount = viewportCount;
	m_viewportState.scissorCount = scissorCount;
}

void VulkanPipelineBuilder::setViewportState(const std::vector<VkViewport>& viewports, const std::vector<VkRect2D>& scissors)
{
	m_viewports = std::vector<VkViewport>(viewports);
	m_scissors = std::vector<VkRect2D>(scissors);
	m_viewportState.viewportCount = static_cast<uint32_t>(m_viewports.size());
	m_viewportState.pViewports = m_viewports.data();
	m_viewportState.scissorCount = static_cast<uint32_t>(m_scissors.size());
	m_viewportState.pScissors = m_scissors.data();
}

void VulkanPipelineBuilder::setRasterizationState(const VkPolygonMode polygonMode, const VkCullModeFlags cullMode, const VkFrontFace frontFace)
{
	m_rasterizationState.polygonMode = polygonMode;
	m_rasterizationState.cullMode = cullMode;
	m_rasterizationState.frontFace = frontFace;
}

void VulkanPipelineBuilder::setMultisampleState(const VkSampleCountFlagBits rasterizationSamples, const VkBool32 sampleShadingEnable, const float minSampleShading)
{
	m_multisampleState.rasterizationSamples = rasterizationSamples;
	m_multisampleState.sampleShadingEnable = sampleShadingEnable;
	m_multisampleState.minSampleShading = minSampleShading;
}

void VulkanPipelineBuilder::setDepthStencilState(const VkBool32 depthTestEnable, const VkBool32 depthWriteEnable, const VkCompareOp depthCompareOp)
{
	m_depthStencilState.depthTestEnable = depthTestEnable;
	m_depthStencilState.depthWriteEnable = depthWriteEnable;
	m_depthStencilState.depthCompareOp = depthCompareOp;
}

void VulkanPipelineBuilder::setColorBlendState(const VkBool32 logicOpEnable, const VkLogicOp logicOp, const std::array<float, 4> colorBlendConstants)
{
	m_colorBlendState.logicOpEnable = logicOpEnable;
	m_colorBlendState.logicOp = logicOp;
	m_colorBlendState.blendConstants[0] = colorBlendConstants[0];
	m_colorBlendState.blendConstants[1] = colorBlendConstants[1];
	m_colorBlendState.blendConstants[2] = colorBlendConstants[2];
	m_colorBlendState.blendConstants[3] = colorBlendConstants[3];
}

void VulkanPipelineBuilder::addColorBlendAttachment(const VkPipelineColorBlendAttachmentState& attachment)
{
	m_attachments.push_back(attachment);
	m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_attachments.size());
	m_colorBlendState.pAttachments = m_attachments.data();
}

void VulkanPipelineBuilder::setDynamicState(const std::vector<VkDynamicState>& dynamicStates)
{
	m_dynamicStates = std::vector<VkDynamicState>(dynamicStates);
	m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
	m_dynamicState.pDynamicStates = m_dynamicStates.data();
}

void VulkanPipelineBuilder::addShaderStage(const uint32_t shader)
{
	m_shaderStages.push_back(shader);
}

void VulkanPipelineBuilder::resetShaderStages()
{
	m_shaderStages.clear();
}

void VulkanPipelineBuilder::setVertexInputState(const VkPipelineVertexInputStateCreateInfo& state)
{
	m_vertexInputState = state;
	m_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_vertexInputBindings.clear();
	m_vertexInputAttributes.clear();
}

void VulkanPipelineBuilder::setInputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& state)
{
	m_inputAssemblyState = state;
	m_inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setTessellationState(const VkPipelineTessellationStateCreateInfo& state)
{
	m_tessellationState = state;
	m_tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	m_tesellationStateEnabled = true;
}

void VulkanPipelineBuilder::setViewportState(const VkPipelineViewportStateCreateInfo& state)
{
	m_viewportState = state;
	m_viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setRasterizationState(const VkPipelineRasterizationStateCreateInfo& state)
{
	m_rasterizationState = state;
	m_rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setMultisampleState(const VkPipelineMultisampleStateCreateInfo& state)
{
	m_multisampleState = state;
	m_multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& state)
{
	m_depthStencilState = state;
	m_depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setColorBlendState(const VkPipelineColorBlendStateCreateInfo& state)
{
	m_colorBlendState = state;
	m_colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_attachments.clear();
}

void VulkanPipelineBuilder::setDynamicState(const VkPipelineDynamicStateCreateInfo& state)
{
	m_dynamicState = state;
	m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanPipelineBuilder::createShaderStages() const
{
	static std::string name = "main";
	std::vector<VkPipelineShaderStageCreateInfo> shaderStagesInfo;
	shaderStagesInfo.reserve(m_shaderStages.size());
	for (const auto& shaderStage : m_shaderStages)
	{
		VkPipelineShaderStageCreateInfo stageInfo{};
		const VulkanShader& shader = m_device->getShader(shaderStage);
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.stage = shader.m_stage;
		stageInfo.module = shader.m_vkHandle;
		stageInfo.pName = name.c_str();
		shaderStagesInfo.push_back(stageInfo);
	}
	return shaderStagesInfo;
}

void VulkanPipeline::free()
{
	if (m_vkHandle != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(m_device->m_vkHandle, m_vkHandle, nullptr);
		m_vkHandle = VK_NULL_HANDLE;
	}
}

uint32_t VulkanPipeline::getLayout() const
{
	return m_layout;
}

uint32_t VulkanPipeline::getRenderPass() const
{
	return m_renderPass;
}

uint32_t VulkanPipeline::getSubpass() const
{
	return m_subpass;
}

VkPipeline VulkanPipeline::operator*() const
{
	return m_vkHandle;
}

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const VkPipeline handle, const uint32_t layout, const uint32_t renderPass, const uint32_t subpass)
	: m_vkHandle(handle), m_layout(layout), m_renderPass(renderPass), m_subpass(subpass), m_device(&device)
{
}

VkPipelineLayout VulkanPipelineLayout::operator*() const
{
	return m_vkHandle;
}

void VulkanPipelineLayout::free()
{
	if (m_vkHandle != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(VulkanContext::getDevice(m_device).m_vkHandle, m_vkHandle, nullptr);
		m_vkHandle = VK_NULL_HANDLE;
	}
}

VulkanPipelineLayout::VulkanPipelineLayout(const uint32_t device, const VkPipelineLayout handle)
	: m_vkHandle(handle), m_device(device)
{
}
