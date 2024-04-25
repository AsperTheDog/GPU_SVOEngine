#include "engine.hpp"

#include <array>
#include <stdexcept>

#include <imgui.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "imgui_internal.h"
#include "utils/logger.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "Octree/octree.hpp"
#include "vulkan_context.hpp"

struct PushConstantData
{
    glm::vec4 camPos;
    glm::mat4 viewProj;
    float scale;
};

VulkanGPU chooseCorrectGPU()
{
	const std::vector<VulkanGPU> gpus = VulkanContext::getGPUs();
	for (auto& gpu : gpus)
	{
		if (gpu.getProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			return gpu;
		}
	}

	throw std::runtime_error("No discrete GPU found");
}

Engine::Engine() : cam({0, 0, 0}, {0, 0, 0}), m_window("Vulkan", 1920, 1080)
{
	Logger::setRootContext("Engine init");
#ifndef _DEBUG
	VulkanContext::init(VK_API_VERSION_1_3, false, false, m_window.getRequiredVulkanExtensions());
#else
	VulkanContext::init(VK_API_VERSION_1_3, true, false, m_window.getRequiredVulkanExtensions());
#endif
	m_window.createSurface(VulkanContext::getHandle());

	const VulkanGPU gpu = chooseCorrectGPU();
	const GPUQueueStructure queueStructure = gpu.getQueueFamilies();
	QueueFamilySelector queueFamilySelector(queueStructure);

	const QueueFamily graphicsQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_GRAPHICS_BIT);
	const QueueFamily presentQueueFamily = queueStructure.findPresentQueueFamily(m_window.getSurface());
	const QueueFamily transferQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_TRANSFER_BIT);

	// Select Queue Families and assign queues
	QueueFamilySelector selector{queueStructure};
	selector.selectQueueFamily(graphicsQueueFamily, QueueFamilyTypeBits::GRAPHICS);
	selector.selectQueueFamily(presentQueueFamily, QueueFamilyTypeBits::PRESENT);
	m_graphicsQueuePos = selector.getOrAddQueue(graphicsQueueFamily, 1.0);
	m_presentQueuePos = selector.getOrAddQueue(presentQueueFamily, 1.0);
	m_transferQueuePos = selector.addQueue(transferQueueFamily, 1.0);

	m_deviceID = VulkanContext::createDevice(gpu, selector, {VK_KHR_SWAPCHAIN_EXTENSION_NAME}, {});
	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

	m_swapchainID = device.createSwapchain(m_window.getSurface(), m_window.getSize().toExtent2D(), {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    const VulkanSwapchain& swapchain = device.getSwapchain(m_swapchainID);

	device.configureOneTimeQueue(m_transferQueuePos);
	m_graphicsCmdBufferID = device.createCommandBuffer(graphicsQueueFamily, 0, false);

	createRenderPass();
	createGraphicsPipeline();

	m_framebuffers.resize(swapchain.getImageCount());
	for (uint32_t i = 0; i < swapchain.getImageCount(); i++)
		m_framebuffers[i] = createFramebuffer(swapchain.getImageView(i), swapchain.getExtent());

	// Create sync objects
	m_renderFinishedSemaphoreID = device.createSemaphore();
	m_inFlightFenceID = device.createFence(true);

	{
		cam.setScreenSize(m_window.getSize().width, m_window.getSize().height);
		cam.setPosition({0.0f, 0.0f, -9.0f});
		cam.lookAt({0.0f, 0.0f, 0.0f});
	}

	setupInputEvents();

	initImgui();
}

Engine::~Engine()
{
	VulkanContext::getDevice(m_deviceID).waitIdle();

	Logger::setRootContext("Resource cleanup");

	ImGui_ImplVulkan_Shutdown();
	m_window.shutdownImgui();
    ImGui::DestroyContext();

	m_window.free();
	VulkanContext::free();
}

void Engine::configureOctreeBuffer(Octree& octree, const float scale)
{
	{
		if (m_octreeBuffer != UINT32_MAX)
		{
			VulkanContext::getDevice(m_deviceID).freeBuffer(m_octreeBuffer);
		}

		const VkDeviceSize bufferSize = octree.getByteSize();

		m_octreeBuffer = VulkanContext::getDevice(m_deviceID).createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		VulkanContext::getDevice(m_deviceID).getBuffer(m_octreeBuffer).allocateFromFlags({VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false});


		bool transientConfig = false;
		if (!VulkanContext::getDevice(m_deviceID).isStagingBufferConfigured())
		{
			transientConfig = true;
			VulkanContext::getDevice(m_deviceID).configureStagingBuffer(100LL * 1024 * 1024, m_transferQueuePos);
		}
		{
            size_t offset = 0;
            const VkDeviceSize stagingBufferSize = VulkanContext::getDevice(m_deviceID).getStagingBufferSize();
            while (offset < octree.getByteSize())
            {
                const VkDeviceSize nextSize = std::min(stagingBufferSize, octree.getByteSize() - offset);
                void* stagePtr = VulkanContext::getDevice(m_deviceID).mapStagingBuffer(nextSize, 0);
			    memcpy(stagePtr, static_cast<char*>(octree.getData()) + offset, nextSize);
			    VulkanContext::getDevice(m_deviceID).dumpStagingBuffer(m_octreeBuffer, nextSize, offset, 0);
                offset += nextSize;
            }
			
		}
		if (transientConfig)
		{
			VulkanContext::getDevice(m_deviceID).freeStagingBuffer();
		}
	}

	{
		if (m_octreeDescrPool != UINT32_MAX)
		{
			VulkanContext::getDevice(m_deviceID).freeDescriptorPool(m_octreeDescrPool);
		}

		m_octreeDescrPool = VulkanContext::getDevice(m_deviceID).createDescriptorPool({{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}, 1, 0);
		m_octreeDescrSet = VulkanContext::getDevice(m_deviceID).createDescriptorSet(m_octreeDescrPool, m_octreeDescrSetLayout);

		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = *VulkanContext::getDevice(m_deviceID).getBuffer(m_octreeBuffer);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = *VulkanContext::getDevice(m_deviceID).getDescriptorSet(m_octreeDescrSet);
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.pBufferInfo = &bufferInfo;

		VulkanContext::getDevice(m_deviceID).getDescriptorSet(m_octreeDescrSet).updateDescriptorSet(writeDescriptorSet);
	}
    m_octreeScale = scale;
}

void Engine::run()
{
	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	VulkanFence& inFlightFence = device.getFence(m_inFlightFenceID);

	const VulkanQueue graphicsQueue = device.getQueue(m_graphicsQueuePos);
	VulkanCommandBuffer& graphicsBuffer = device.getCommandBuffer(m_graphicsCmdBufferID, 0);

	uint64_t frameCounter = 0;

	while (!m_window.shouldClose())
	{
	    Logger::setRootContext("Frame " + std::to_string(frameCounter));
		m_window.pollEvents();

		inFlightFence.wait();
		inFlightFence.reset();

		const uint32_t nextImage = device.getSwapchain(m_swapchainID).acquireNextImage();

		if (nextImage == UINT32_MAX)
		{
			frameCounter++;
			continue;
		}

		ImGui_ImplVulkan_NewFrame();
	    m_window.frameImgui();
	    ImGui::NewFrame();

		Engine::drawImgui();

		ImGui::Render();
		ImDrawData* imguiDrawData = ImGui::GetDrawData();

		if (imguiDrawData->DisplaySize.x <= 0.0f || imguiDrawData->DisplaySize.y <= 0.0f)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			continue;
		}

		recordCommandBuffer(m_framebuffers[nextImage], imguiDrawData);

		graphicsBuffer.submit(graphicsQueue, {{device.getSwapchain(m_swapchainID).getImgSemaphore(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}}, {m_renderFinishedSemaphoreID}, m_inFlightFenceID);

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		device.getSwapchain(m_swapchainID).present(m_presentQueuePos, {m_renderFinishedSemaphoreID});

		frameCounter++;
	}
}

void Engine::createRenderPass()
{
	Logger::pushContext("Create RenderPass");
	VulkanRenderPassBuilder builder{};

    const VkFormat format = VulkanContext::getDevice(m_deviceID).getSwapchain(m_swapchainID).getFormat().format;

	const VkAttachmentDescription colorAttachment = VulkanRenderPassBuilder::createAttachment(format, 
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	builder.addAttachment(colorAttachment);

	builder.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, {{COLOR, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}, 0);

	m_renderPassID = VulkanContext::getDevice(m_deviceID).createRenderPass(builder, 0);
	Logger::popContext();
}

void Engine::createGraphicsPipeline()
{
	Logger::pushContext("Create Pipeline");

	VkDescriptorSetLayoutBinding octreeBinding{};
	octreeBinding.binding = 0;
	octreeBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	octreeBinding.descriptorCount = 1;
	octreeBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	m_octreeDescrSetLayout = VulkanContext::getDevice(m_deviceID).createDescriptorSetLayout({octreeBinding}, 0);

	std::vector<VkPushConstantRange> pushConstants{1};
	pushConstants[0] = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData)};
	const uint32_t layout = VulkanContext::getDevice(m_deviceID).createPipelineLayout({m_octreeDescrSetLayout}, pushConstants);

	const uint32_t vertexShader = VulkanContext::getDevice(m_deviceID).createShader("shaders/raytracing.vert", VK_SHADER_STAGE_VERTEX_BIT);
	const uint32_t fragmentShader = VulkanContext::getDevice(m_deviceID).createShader("shaders/raytracing.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VulkanPipelineBuilder builder{&VulkanContext::getDevice(m_deviceID)};

	builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
	builder.setViewportState(1, 1);
	builder.setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	builder.setMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f);
	builder.setDepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
	builder.addColorBlendAttachment(colorBlendAttachment);
	builder.setColorBlendState(VK_FALSE, VK_LOGIC_OP_COPY, {0.0f, 0.0f, 0.0f, 0.0f});
	builder.setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
	builder.addShaderStage(vertexShader);
	builder.addShaderStage(fragmentShader);
	m_pipelineID = VulkanContext::getDevice(m_deviceID).createPipeline(builder, layout, m_renderPassID, 0);
	Logger::popContext();
}

uint32_t Engine::createFramebuffer(const VkImageView colorAttachment, const VkExtent2D newExtent) const
{
	const std::vector<VkImageView> attachments{colorAttachment};
	return VulkanContext::getDevice(m_deviceID).createFramebuffer({newExtent.width, newExtent.height, 1}, VulkanContext::getDevice(m_deviceID).getRenderPass(m_renderPassID), attachments);
}

void Engine::initImgui() const
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

	const std::vector<VkDescriptorPoolSize> pool_sizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
	const uint32_t imguiPoolID = device.createDescriptorPool(pool_sizes, 1000U * pool_sizes.size(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

    const VulkanSwapchain& swapchain = device.getSwapchain(m_swapchainID);

    m_window.initImgui();
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = VulkanContext::getHandle();
    init_info.PhysicalDevice = *device.getGPU();
    init_info.Device = *device;
    init_info.QueueFamily = m_graphicsQueuePos.familyIndex;
    init_info.Queue = *device.getQueue(m_graphicsQueuePos);
    init_info.DescriptorPool = *device.getDescriptorPool(imguiPoolID);
    init_info.RenderPass = *device.getRenderPass(m_renderPassID);
    init_info.Subpass = 0;
    init_info.MinImageCount = swapchain.getMinImageCount();
    init_info.ImageCount = swapchain.getImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info);
}

void Engine::setupInputEvents()
{
	m_window.getMouseMovedSignal().connect(&cam, &Camera::mouseMoved);
	m_window.getKeyPressedSignal().connect(&cam, &Camera::keyPressed);
	m_window.getKeyReleasedSignal().connect(&cam, &Camera::keyReleased);
	m_window.getEventsProcessedSignal().connect(&cam, &Camera::updateEvents);
	m_window.getKeyPressedSignal().connect([&](const uint32_t key)
	{
		if (key == SDLK_g)
		{
			m_window.toggleMouseCapture();
		}
	});

    m_window.getResizedSignal().connect([&](const VkExtent2D extent)
    {
        VulkanContext::getDevice(m_deviceID).waitIdle();
        VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

        m_swapchainID = device.createSwapchain(m_window.getSurface(), extent, device.getSwapchain(m_swapchainID).getFormat(), m_swapchainID);
        const VulkanSwapchain& swapchain = device.getSwapchain(m_swapchainID);

        Logger::pushContext("Swapchain resources rebuild");
		for (uint32_t i = 0; i < swapchain.getImageCount(); i++)
		{
			device.freeFramebuffer(m_framebuffers[i]);
			m_framebuffers[i] = createFramebuffer(swapchain.getImageView(i), extent);
		}
		Logger::popContext();
    });
}

void Engine::recordCommandBuffer(const uint32_t framebufferID, ImDrawData* main_draw_data)
{
	Logger::pushContext("Command buffer recording");
    const VulkanSwapchain& swapchain = VulkanContext::getDevice(m_deviceID).getSwapchain(m_swapchainID);

	std::vector<VkClearValue> clearValues{2};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

	VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

	VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = swapchain.getExtent();

	const uint32_t layout = VulkanContext::getDevice(m_deviceID).getPipeline(m_pipelineID).getLayout();

	const Camera::Data camData = cam.getData();
    const PushConstantData pushConstants = {camData.position, camData.invPVMatrix, m_octreeScale};

	VulkanCommandBuffer& graphicsBuffer = VulkanContext::getDevice(m_deviceID).getCommandBuffer(m_graphicsCmdBufferID, 0);
	graphicsBuffer.reset();
	graphicsBuffer.beginRecording();

	graphicsBuffer.cmdBeginRenderPass(m_renderPassID, framebufferID, swapchain.getExtent(), clearValues);

        graphicsBuffer.cmdBindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, m_octreeDescrSet);

		graphicsBuffer.cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineID);
		graphicsBuffer.cmdSetViewport(viewport);
		graphicsBuffer.cmdSetScissor(scissor);

		graphicsBuffer.cmdPushConstant(layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);
		graphicsBuffer.cmdDraw(6, 0);

		ImGui_ImplVulkan_RenderDrawData(main_draw_data, *graphicsBuffer);

	graphicsBuffer.cmdEndRenderPass();
	graphicsBuffer.endRecording();

	Logger::popContext();
}

void Engine::drawImgui() const
{
	const ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("Metrics");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

	ImGui::End();
}