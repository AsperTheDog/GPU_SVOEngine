#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "vulkan_base.hpp"
#include "vulkan_binding.hpp"

class VulkanDevice;

struct VulkanPipelineBuilder
{
	explicit VulkanPipelineBuilder(VulkanDevice* device);

	void addShaderStage(uint32_t shader);
	void resetShaderStages();

	void setVertexInputState(const VkPipelineVertexInputStateCreateInfo& state);
	void addVertexBinding(const VulkanBinding& binding);

	void setInputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& state);
	void setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);

	void setTessellationState(const VkPipelineTessellationStateCreateInfo& state);
	void setTessellationState(uint32_t patchControlPoints);

	void setViewportState(const VkPipelineViewportStateCreateInfo& state);
	void setViewportState(uint32_t viewportCount, uint32_t scissorCount);
	void setViewportState(const std::vector<VkViewport>& viewports, const std::vector<VkRect2D>& scissors);

	void setRasterizationState(const VkPipelineRasterizationStateCreateInfo& state);
	void setRasterizationState(VkPolygonMode polygonMode, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE);

	void setMultisampleState(const VkPipelineMultisampleStateCreateInfo& state);
	void setMultisampleState(VkSampleCountFlagBits rasterizationSamples, VkBool32 sampleShadingEnable = VK_FALSE, float minSampleShading = 1.0f);

	void setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& state);
	void setDepthStencilState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp);

	void setColorBlendState(const VkPipelineColorBlendStateCreateInfo& state);
	void setColorBlendState(VkBool32 logicOpEnable, VkLogicOp logicOp, std::array<float, 4> colorBlendConstants);
	void addColorBlendAttachment(const VkPipelineColorBlendAttachmentState& attachment);

	void setDynamicState(const VkPipelineDynamicStateCreateInfo& state);
	void setDynamicState(const std::vector<VkDynamicState>& dynamicStates);


private:
	VkPipelineVertexInputStateCreateInfo m_vertexInputState{};
	VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyState{};
	VkPipelineTessellationStateCreateInfo m_tessellationState{};
	VkPipelineViewportStateCreateInfo m_viewportState{};
	VkPipelineRasterizationStateCreateInfo m_rasterizationState{};
	VkPipelineMultisampleStateCreateInfo m_multisampleState{};
	VkPipelineDepthStencilStateCreateInfo m_depthStencilState{};
	VkPipelineColorBlendStateCreateInfo m_colorBlendState{};
	VkPipelineDynamicStateCreateInfo m_dynamicState{};

	bool m_tesellationStateEnabled = false;

	std::vector<uint32_t> m_shaderStages;
	std::vector<VkVertexInputBindingDescription> m_vertexInputBindings;
	std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributes;
	std::vector<VkViewport> m_viewports;
	std::vector<VkRect2D> m_scissors;
	std::vector<VkPipelineColorBlendAttachmentState> m_attachments;
	std::vector<VkDynamicState> m_dynamicStates;

	VulkanDevice* m_device;

	[[nodiscard]] std::vector<VkPipelineShaderStageCreateInfo> createShaderStages() const;

	friend class VulkanDevice;
};

class VulkanPipeline : public VulkanBase
{
public:
	[[nodiscard]] uint32_t getLayout() const;
	[[nodiscard]] uint32_t getRenderPass() const;
	[[nodiscard]] uint32_t getSubpass() const;

private:
	void free();

	VulkanPipeline() = default;
	VulkanPipeline(VulkanDevice& device, VkPipeline handle, uint32_t layout, uint32_t renderPass, uint32_t subpass);

	VkPipeline m_vkHandle;

	uint32_t m_layout;
	uint32_t m_renderPass;
	uint32_t m_subpass;

	VulkanDevice* m_device;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};

class VulkanPipelineLayout : public VulkanBase
{
private:
	void free();

	VulkanPipelineLayout(uint32_t device, VkPipelineLayout handle);

	VkPipelineLayout m_vkHandle = VK_NULL_HANDLE;

	uint32_t m_device;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};