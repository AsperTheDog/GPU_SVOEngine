#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

#include "vulkan_base.hpp"

class VulkanDevice;

enum AttachmentType
{
	COLOR,
	DEPTH_STENCIL,
	INPUT,
	RESOLVE,
	PRESERVE
};

class VulkanRenderPassBuilder
{
public:
	struct AttachmentReference
	{
		AttachmentType type;
		uint32_t attachment;
		VkImageLayout layout;
	};

	VulkanRenderPassBuilder& addAttachment(const VkAttachmentDescription& attachment);
	VulkanRenderPassBuilder& addSubpass(VkPipelineBindPoint bindPoint, const std::vector<AttachmentReference>& attachments, VkSubpassDescriptionFlags flags);
	VulkanRenderPassBuilder& addDependency(const VkSubpassDependency& dependency);

	static VkAttachmentDescription createAttachment(VkFormat format, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initialLayout, VkImageLayout finalLayout);


private:
	struct SubpassInfo
	{
		VkPipelineBindPoint bindPoint;
		VkSubpassDescriptionFlags flags;
		std::vector<VkAttachmentReference> colorAttachments;
		std::vector<VkAttachmentReference> resolveAttachments;
		std::vector<VkAttachmentReference> inputAttachments;
		VkAttachmentReference depthStencilAttachment;
		std::vector<uint32_t> preserveAttachments;
		bool hasDepthStencilAttachment = false;
	};

	std::vector<VkAttachmentDescription> m_attachments;
	std::vector<SubpassInfo> m_subpasses;
	std::vector<VkSubpassDependency> m_dependencies;

	friend class VulkanDevice;
};

class VulkanRenderPass : public VulkanBase
{
public:

private:
	void free();

	VulkanRenderPass(uint32_t device, VkRenderPass renderPass);

	VkRenderPass m_vkHandle = VK_NULL_HANDLE;
	uint32_t m_device;

	friend class VulkanDevice;
	friend class VulkanCommandBuffer;
};

