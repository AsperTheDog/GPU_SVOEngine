#pragma once
#include "camera.hpp"
#include "imgui.h"
#include "sdl_window.hpp"
#include "vulkan_queues.hpp"
#include "vulkan_shader.hpp"

class Octree;

class Engine
{
public:
    explicit Engine(uint32_t samplerImageCount, uint8_t depth);
	~Engine();

	void configureOctreeBuffer(Octree& octree, float scale);

	void run();

private:
	void createRenderPass();
    uint32_t createGraphicsPipeline(const uint32_t samplerImageCount, const std::string& fragmentShader, std::vector<VulkanShader::MacroDef> macros);
	uint32_t createFramebuffer(VkImageView colorAttachment, VkExtent2D newExtent) const;
	void initImgui() const;

	void setupInputEvents();

	void recordCommandBuffer(uint32_t framebufferID, ImDrawData* main_draw_data);

	void drawImgui();

    void updatePipelines();

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
	uint32_t m_noShadowPipelineID = UINT32_MAX;
	uint32_t m_intersectPipelineID = UINT32_MAX;
	uint32_t m_intersectColorPipelineID = UINT32_MAX;
    uint32_t m_pipelineLayoutID = UINT32_MAX;
	std::vector<uint32_t> m_framebuffers{};
	uint32_t m_renderFinishedSemaphoreID = UINT32_MAX;
	uint32_t m_inFlightFenceID = UINT32_MAX;

	uint32_t m_octreeBuffer = UINT32_MAX;
	uint32_t m_octreeDescrPool = UINT32_MAX;
	uint32_t m_octreeDescrSetLayout = UINT32_MAX;
	uint32_t m_octreeDescrSet = UINT32_MAX;
    VkDeviceSize m_octreeBufferSize = 0;
    VkDeviceSize m_octreeMatPadding = 0;
    float m_octreeScale = 1.0f;
    float m_sunRotationLat = 0.0f;
    float m_sunRotationAlt = 0.0f;
    glm::vec3 m_sunlightDir{1.0f, 1.0f, 0.0f};
    glm::vec3 m_skyColor{0.0, 1.0, 1.0};
    glm::vec3 m_sunColor{1.0, 1.0, 1.0};
    std::vector<std::pair<uint32_t, VkSampler>> m_octreeImages{};

    uint32_t m_samplerImageCount = 0;
    VkDeviceSize m_octreeImagesMemUsage = 0;
    float m_voxelSize = 1.0f;
    uint8_t m_depth = 0;

    Octree* m_octree = nullptr;

    bool m_noShadows = true;
    bool m_intersectionTest = false;
    bool m_intersectionTestColor = false;

    float m_brightness = 0.0f;
    float m_saturation = 1.0f;
    float m_contrast = 1.0f;
    float m_gamma = 1.0f;
};

