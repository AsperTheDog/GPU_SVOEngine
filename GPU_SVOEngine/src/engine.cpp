#include "engine.hpp"

#include <array>
#include <stdexcept>

#include <imgui.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "logger.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "VkBase/vulkan_context.hpp"

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
	VulkanContext::init(VK_API_VERSION_1_3, false, m_window.getRequiredVulkanExtensions());
#else
	VulkanContext::init(VK_API_VERSION_1_3, true, m_window.getRequiredVulkanExtensions());
#endif
	m_window.createSurface();

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
	QueueSelection transferQueuePos = selector.addQueue(transferQueueFamily, 1.0);

	m_deviceID = VulkanContext::createDevice(gpu, selector, {VK_KHR_SWAPCHAIN_EXTENSION_NAME}, {});
	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

	m_window.createSwapchain(m_deviceID, {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

	device.configureOneTimeQueue(transferQueuePos);
	m_graphicsCmdBufferID = device.createCommandBuffer(graphicsQueueFamily, 0, false);

	createRenderPass();
	createGraphicsPipeline();

	device.configureStagingBuffer(5LL * 1024 * 1024, transferQueuePos);

	m_framebuffers.resize(m_window.getImageCount());
	for (uint32_t i = 0; i < m_window.getImageCount(); i++)
		m_framebuffers[i] = createFramebuffer(m_window.getImageView(i));

	// Create sync objects
	m_imageAvailableSemaphoreID = device.createSemaphore();
	m_renderFinishedSemaphoreID = device.createSemaphore();
	m_inFlightFenceID = device.createFence(true);

	initImgui();

	{
		cam.setScreenSize(m_window.getSize().width, m_window.getSize().height);
		cam.setPosition({0.0f, 0.0f, -9.0f});
		cam.lookAt({0.0f, 0.0f, 0.0f});
	}
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

void Engine::configureOctreeBuffer(Octree& octree)
{

}

void Engine::run()
{
	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	VulkanFence& inFlightFence = device.getFence(m_inFlightFenceID);

	const VulkanQueue graphicsQueue = device.getQueue(m_graphicsQueuePos);
	const VulkanQueue presentQueue = device.getQueue(m_presentQueuePos);
	const VulkanCommandBuffer& graphicsBuffer = device.getCommandBuffer(m_graphicsCmdBufferID, 0);

	uint64_t frameCounter = 0;
	Logger::setRootContext("Frame" + std::to_string(frameCounter));

	while (!m_window.shouldClose())
	{
		m_window.pollEvents();
		inFlightFence.wait();

		if (m_window.getAndResetSwapchainRebuildFlag())
		{
			Logger::pushContext("Swapchain resources rebuild");
			for (uint32_t i = 0; i < m_window.getImageCount(); i++)
			{
				device.freeFramebuffer(m_framebuffers[i]);
				m_framebuffers[i] = createFramebuffer(m_window.getImageView(i));
			}
			Logger::popContext();
		}

		const uint32_t nextImage = m_window.acquireNextImage(m_imageAvailableSemaphoreID, nullptr);

		inFlightFence.reset();

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

		graphicsBuffer.submit(graphicsQueue, {{m_imageAvailableSemaphoreID, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}}, {m_renderFinishedSemaphoreID}, m_inFlightFenceID);


		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		m_window.present(presentQueue, nextImage, m_renderFinishedSemaphoreID);

		frameCounter++;
	}
}

void Engine::createRenderPass()
{
	Logger::pushContext("Create RenderPass");
	VulkanRenderPassBuilder builder{};

	const VkAttachmentDescription colorAttachment = VulkanRenderPassBuilder::createAttachment(m_window.getSwapchainImageFormat().format, 
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	builder.addAttachment(colorAttachment);

	builder.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, {{COLOR, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}, 0);

	m_renderPassID = VulkanContext::getDevice(m_deviceID).createRenderPass(builder, 0);
	Logger::popContext();
}

void Engine::createGraphicsPipeline()
{
	Logger::pushContext("Create Pipeline");
	std::vector<VkPushConstantRange> pushConstants{1};
	pushConstants[0] = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4) + sizeof(glm::mat4)};
	const uint32_t layout = VulkanContext::getDevice(m_deviceID).createPipelineLayout({}, pushConstants);

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

uint32_t Engine::createFramebuffer(const VkImageView colorAttachment) const
{
	const std::vector<VkImageView> attachments{colorAttachment};
	const VkExtent2D extent = m_window.getSwapchainExtent();
	return VulkanContext::getDevice(m_deviceID).createFramebuffer({extent.width, extent.height, 1}, VulkanContext::getDevice(m_deviceID).getRenderPass(m_renderPassID), attachments);
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
	const uint32_t imguiPoolID = device.createDescriptorPool(pool_sizes, 1000 * pool_sizes.size(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

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
    init_info.MinImageCount = m_window.getMinImageCount();
    init_info.ImageCount = m_window.getImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info);
}

void Engine::recordCommandBuffer(const uint32_t framebufferID, ImDrawData* main_draw_data)
{
	Logger::pushContext("Command buffer recording");

	std::vector<VkClearValue> clearValues{2};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

	VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_window.getSwapchainExtent().width);
    viewport.height = static_cast<float>(m_window.getSwapchainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

	VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_window.getSwapchainExtent();

	const uint32_t layout = VulkanContext::getDevice(m_deviceID).getPipeline(m_pipelineID).getLayout();

	const Camera::Data camData = cam.getData();

	VulkanCommandBuffer& graphicsBuffer = VulkanContext::getDevice(m_deviceID).getCommandBuffer(m_graphicsCmdBufferID, 0);
	graphicsBuffer.reset();
	graphicsBuffer.beginRecording();

	graphicsBuffer.cmdBeginRenderPass(m_renderPassID, framebufferID, m_window.getSwapchainExtent(), clearValues);

		graphicsBuffer.cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineID);
		graphicsBuffer.cmdSetViewport(viewport);
		graphicsBuffer.cmdSetScissor(scissor);

		graphicsBuffer.cmdPushConstant(layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(camData), &camData);
		graphicsBuffer.cmdDraw(6, 0);

		ImGui_ImplVulkan_RenderDrawData(main_draw_data, *graphicsBuffer);

	graphicsBuffer.cmdEndRenderPass();
	graphicsBuffer.endRecording();

	Logger::popContext();
}

void Engine::drawImgui() const
{
	ImGui::ShowDemoWindow();
}
