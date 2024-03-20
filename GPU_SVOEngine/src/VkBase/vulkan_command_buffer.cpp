#include "VkBase/vulkan_command_buffer.hpp"

#include <array>
#include <stdexcept>
#include <vector>

#include "VkBase/vulkan_buffer.hpp"
#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"
#include "VkBase/vulkan_sync.hpp"
#include "VkBase/vulkan_framebuffer.hpp"
#include "VkBase/vulkan_pipeline.hpp"
#include "VkBase/vulkan_queues.hpp"
#include "VkBase/vulkan_render_pass.hpp"

void VulkanCommandBuffer::beginRecording(const VkCommandBufferUsageFlags flags)
{
	if (m_isRecording)
	{
		throw std::runtime_error("Command buffer is already recording");
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = flags;

	vkBeginCommandBuffer(m_vkHandle, &beginInfo);

	m_isRecording = true;
}

void VulkanCommandBuffer::endRecording()
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkEndCommandBuffer(m_vkHandle);

	m_isRecording = false;
}

void VulkanCommandBuffer::cmdCopyBuffer(const uint32_t source, const uint32_t destination, const std::vector<VkBufferCopy>& copyRegions) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}
	
	VulkanDevice& device = VulkanContext::getDevice(m_device);
	vkCmdCopyBuffer(m_vkHandle, device.getBuffer(source).m_vkHandle, device.getBuffer(destination).m_vkHandle, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
}

void VulkanCommandBuffer::cmdPushConstant(const uint32_t layout, const VkShaderStageFlags stageFlags, const uint32_t offset, const uint32_t size, const void* pValues) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdPushConstants(m_vkHandle, VulkanContext::getDevice(m_device).getPipelineLayout(layout).m_vkHandle, stageFlags, offset, size, pValues);
}

void VulkanCommandBuffer::submit(const VulkanQueue& queue, const std::vector<std::pair<uint32_t, VkSemaphoreWaitFlags>>& waitSemaphoreData, const std::vector<uint32_t>& signalSemaphores, const uint32_t fence) const
{
	if (m_isRecording)
	{
		throw std::runtime_error("Command buffer is still recording");
	}

	VulkanDevice& device = VulkanContext::getDevice(m_device);

	std::vector<VkSemaphore> waitSemaphores{};
	std::vector<VkPipelineStageFlags> waitStages{};
	waitSemaphores.resize(waitSemaphoreData.size());
	waitStages.resize(waitSemaphoreData.size());
	for (size_t i = 0; i < waitSemaphores.size(); i++)
	{
		waitSemaphores[i] = device.getSemaphore(waitSemaphoreData[i].first).m_vkHandle;
		waitStages[i] = waitSemaphoreData[i].second;
	}

	std::vector<VkSemaphore> signalSemaphoresVk{};
	signalSemaphoresVk.reserve(signalSemaphores.size());
	for (const auto& semaphore : signalSemaphores)
	{
		signalSemaphoresVk.push_back(device.getSemaphore(semaphore).m_vkHandle);
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vkHandle;
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphoresVk.size());
	submitInfo.pSignalSemaphores = signalSemaphoresVk.data();

	vkQueueSubmit(queue.m_vkHandle, 1, &submitInfo, fence != UINT32_MAX ? device.getFence(fence).m_vkHandle : VK_NULL_HANDLE);
}

void VulkanCommandBuffer::reset() const
{
	if (m_isRecording)
	{
		throw std::runtime_error("Command buffer is still recording");
	}

	vkResetCommandBuffer(m_vkHandle, 0);
}

void VulkanCommandBuffer::cmdBeginRenderPass(const uint32_t renderPass, const uint32_t frameBuffer, const VkExtent2D extent, const std::vector<VkClearValue>& clearValues) const
{
	VkRenderPassBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = VulkanContext::getDevice(m_device).getRenderPass(renderPass).m_vkHandle;
	beginInfo.framebuffer = VulkanContext::getDevice(m_device).getFramebuffer(frameBuffer).m_vkHandle;
	beginInfo.renderArea.offset = { 0, 0 };
	beginInfo.renderArea.extent = extent;

    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	beginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_vkHandle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::cmdEndRenderPass() const
{
	vkCmdEndRenderPass(m_vkHandle);
}

void VulkanCommandBuffer::cmdBindPipeline(const VkPipelineBindPoint bindPoint, const uint32_t pipelineID) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdBindPipeline(m_vkHandle, bindPoint, VulkanContext::getDevice(m_device).getPipeline(pipelineID).m_vkHandle);
}

void VulkanCommandBuffer::cmdNextSubpass() const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdNextSubpass(m_vkHandle, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::cmdPipelineBarrier(const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask, const VkDependencyFlags dependencyFlags, 
	const std::vector<VkMemoryBarrier>& memoryBarriers,
	const std::vector<VkBufferMemoryBarrier>& bufferMemoryBarriers,
	const std::vector<VkImageMemoryBarrier>& imageMemoryBarriers) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdPipelineBarrier(m_vkHandle, srcStageMask, dstStageMask, dependencyFlags, static_cast<uint32_t>(memoryBarriers.size()), memoryBarriers.data(), static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
}

void VulkanCommandBuffer::cmdBindVertexBuffer(const uint32_t buffer, const VkDeviceSize offset) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdBindVertexBuffers(m_vkHandle, 0, 1, &VulkanContext::getDevice(m_device).getBuffer(buffer).m_vkHandle, &offset);
}

void VulkanCommandBuffer::cmdBindVertexBuffers(const std::vector<uint32_t>& bufferIDs, const std::vector<VkDeviceSize>& offsets) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	std::vector<VkBuffer> vkBuffers;
	vkBuffers.reserve(bufferIDs.size());
	for (const auto& buffer : bufferIDs)
	{
		vkBuffers.push_back(VulkanContext::getDevice(m_device).getBuffer(buffer).m_vkHandle);
	}
	vkCmdBindVertexBuffers(m_vkHandle, 0, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
}

void VulkanCommandBuffer::cmdBindIndexBuffer(const uint32_t bufferID, const VkDeviceSize offset, const VkIndexType indexType) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdBindIndexBuffer(m_vkHandle, VulkanContext::getDevice(m_device).getBuffer(bufferID).m_vkHandle, offset, indexType);
}

void VulkanCommandBuffer::cmdSetViewport(const VkViewport& viewport) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdSetViewport(m_vkHandle, 0, 1, &viewport);
}

void VulkanCommandBuffer::cmdSetScissor(const VkRect2D scissor) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdSetScissor(m_vkHandle, 0, 1, &scissor);
}

void VulkanCommandBuffer::cmdDraw(const uint32_t vertexCount, const uint32_t firstVertex) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}

	vkCmdDraw(m_vkHandle, vertexCount, 1, firstVertex, 0);
}

void VulkanCommandBuffer::cmdDrawIndexed(const uint32_t indexCount, const uint32_t  firstIndex, const int32_t  vertexOffset) const
{
	if (!m_isRecording)
	{
		throw std::runtime_error("Command buffer is not recording");
	}
	vkCmdDrawIndexed(m_vkHandle, indexCount, 1, firstIndex, vertexOffset, 0);
}

VkCommandBuffer VulkanCommandBuffer::operator*() const
{
	return m_vkHandle;
}

VulkanCommandBuffer::VulkanCommandBuffer(const uint32_t device, const VkCommandBuffer commandBuffer, const bool isSecondary, const uint32_t familyIndex, const uint32_t threadID)
	: m_vkHandle(commandBuffer), m_isSecondary(isSecondary), m_familyIndex(familyIndex), m_threadID(threadID), m_device(device)
{
}
