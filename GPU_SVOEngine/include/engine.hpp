#pragma once
#include "imgui.h"
#include "VkBase/sdl_window.hpp"
#include "VkBase/vulkan_queues.hpp"

class Engine
{
public:
	Engine();
	~Engine();

	void run();

private:
	void createRenderPass();
	void createGraphicsPipeline();
	uint32_t createFramebuffer(VkImageView colorAttachment) const;
	void initImgui() const;

	void recordCommandBuffer(const uint32_t framebufferID, ImDrawData* main_draw_data) const;

	void drawImgui() const;

	SDLWindow m_window;

	uint32_t m_deviceID = UINT32_MAX;

	QueueSelection m_graphicsQueuePos{};
	QueueSelection m_presentQueuePos{};

	uint32_t m_graphicsCmdBufferID = UINT32_MAX;
	uint32_t m_renderPassID = UINT32_MAX;
	uint32_t m_pipelineID = UINT32_MAX;
	std::vector<uint32_t> m_framebuffers{};
	uint32_t m_imageAvailableSemaphoreID = UINT32_MAX;
	uint32_t m_renderFinishedSemaphoreID = UINT32_MAX;
	uint32_t m_inFlightFenceID = UINT32_MAX;
};

