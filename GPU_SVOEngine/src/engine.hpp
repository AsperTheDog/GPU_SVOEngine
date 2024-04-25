#pragma once
#include "camera.hpp"
#include "imgui.h"
#include "sdl_window.hpp"
#include "vulkan_queues.hpp"

class Octree;

class Engine
{
public:
	Engine();
	~Engine();

	void configureOctreeBuffer(Octree& octree, float scale);

	void run();

private:
	void createRenderPass();
	void createGraphicsPipeline();
	uint32_t createFramebuffer(const VkImageView colorAttachment, const VkExtent2D newExtent) const;
	void initImgui() const;

	void setupInputEvents();

	void recordCommandBuffer(uint32_t framebufferID, ImDrawData* main_draw_data);

	void drawImgui() const;

	Camera cam;

	SDLWindow m_window;
    uint32_t m_swapchainID = UINT32_MAX;

	uint32_t m_deviceID = UINT32_MAX;

	QueueSelection m_graphicsQueuePos{};
	QueueSelection m_presentQueuePos{};
	QueueSelection m_transferQueuePos{};

	uint32_t m_graphicsCmdBufferID = UINT32_MAX;
	uint32_t m_renderPassID = UINT32_MAX;
	uint32_t m_pipelineID = UINT32_MAX;
	std::vector<uint32_t> m_framebuffers{};
	uint32_t m_renderFinishedSemaphoreID = UINT32_MAX;
	uint32_t m_inFlightFenceID = UINT32_MAX;

	uint32_t m_octreeBuffer = UINT32_MAX;
	uint32_t m_octreeDescrPool = UINT32_MAX;
	uint32_t m_octreeDescrSetLayout = UINT32_MAX;
	uint32_t m_octreeDescrSet = UINT32_MAX;
    float m_octreeScale = 1.0f;
};

