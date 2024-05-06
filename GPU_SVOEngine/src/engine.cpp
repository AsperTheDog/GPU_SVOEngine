#include "engine.hpp"

#include <array>
#include <stdexcept>

#include <imgui.h>
#include <ranges>

#include <glm/gtx/quaternion.hpp>

#include "imgui_internal.h"
#include "backends/imgui_impl_vulkan.h"

#include "utils/logger.hpp"

#include "vulkan_context.hpp"
#include "Octree/octree.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Push constant data for the shaders
// The alignment rules for the GPU are specified here (https://registry.khronos.org/OpenGL/extensions/ARB/ARB_uniform_buffer_object.txt)
// Search for the word "alignment" and it will jump to the correct section
// As a general tip: Floats and uints are aligned to 4 bytes. Vec3, vec4 and mat4 are aligned to 16 bytes
struct PushConstantData
{
    alignas(16) glm::vec3 camPos;
    alignas(16) glm::mat4 viewProj;
    alignas(16) glm::vec3 sunDirection;
    alignas(16) glm::vec3 skyColor;
    alignas(16) glm::vec3 sunColor;
    alignas(4) float scale;
    alignas(4) float brightness;
    alignas(4) float saturation;
    alignas(4) float contrast;
    alignas(4) float gamma;
};

// Simple helper function to choose the correct GPU. Right now it just tries to look for a discrete GPU
// It should also check for capabilities and limits but it's incredibly rare that a moderately modern discrete GPU doesn't support what we need
static VulkanGPU chooseCorrectGPU()
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

// The constructor will all Vulkan resources and initialize ImGui. Not much to see here
Engine::Engine(const uint32_t samplerImageCount, const uint8_t depth) : cam({ 0, 0, 0 }, { 0, 0, 0 }), m_window("Vulkan", 1920, 1080)
{
    // Vulkan Instance
    Logger::setRootContext("Engine init");
#ifndef _DEBUG
    VulkanContext::init(VK_API_VERSION_1_3, false, false, m_window.getRequiredVulkanExtensions());
#else
    VulkanContext::init(VK_API_VERSION_1_3, true, false, m_window.getRequiredVulkanExtensions());
#endif
    // Vulkan Surface
    m_window.createSurface(VulkanContext::getHandle());

    // Choose Physical Device
    const VulkanGPU gpu = chooseCorrectGPU();

    // Select Queue Families
    const GPUQueueStructure queueStructure = gpu.getQueueFamilies();
    QueueFamilySelector queueFamilySelector(queueStructure);

    const QueueFamily graphicsQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    const QueueFamily presentQueueFamily = queueStructure.findPresentQueueFamily(m_window.getSurface());
    const QueueFamily transferQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_TRANSFER_BIT);

    // Select Queue Families and assign queues
    QueueFamilySelector selector{ queueStructure };
    selector.selectQueueFamily(graphicsQueueFamily, QueueFamilyTypeBits::GRAPHICS);
    selector.selectQueueFamily(presentQueueFamily, QueueFamilyTypeBits::PRESENT);
    m_graphicsQueuePos = selector.getOrAddQueue(graphicsQueueFamily, 1.0);
    m_presentQueuePos = selector.getOrAddQueue(presentQueueFamily, 1.0);
    m_transferQueuePos = selector.addQueue(transferQueueFamily, 1.0);

    // Logical Device
    m_deviceID = VulkanContext::createDevice(gpu, selector, { VK_KHR_SWAPCHAIN_EXTENSION_NAME }, {});
    VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

    // Swapchain
    m_swapchainID = device.createSwapchain(m_window.getSurface(), m_window.getSize().toExtent2D(), { VK_FORMAT_R8G8B8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR });
    const VulkanSwapchain& swapchain = device.getSwapchain(m_swapchainID);

    // Command Buffers
    device.configureOneTimeQueue(m_transferQueuePos);
    m_graphicsCmdBufferID = device.createCommandBuffer(graphicsQueueFamily, 0, false);

    m_depth = depth;
    m_voxelSize = std::sqrt(2.0f) * (1.0f / static_cast<float>(1 << m_depth)) / 2.0f;
    m_samplerImageCount = std::max(samplerImageCount, 1U);

    // Renderpass and pipelines
    createRenderPass();
    Engine::updatePipelines();

    // Framebuffers
    m_framebuffers.resize(swapchain.getImageCount());
    for (uint32_t i = 0; i < swapchain.getImageCount(); i++)
        m_framebuffers[i] = createFramebuffer(swapchain.getImageView(i), swapchain.getExtent());

    // Create sync objects
    m_renderFinishedSemaphoreID = device.createSemaphore();
    m_inFlightFenceID = device.createFence(true);

    {
        cam.setScreenSize(m_window.getSize().width, m_window.getSize().height);
        cam.setPosition({ 0.0f, 0.0f, -9.0f });
        cam.lookAt({ 0.0f, 0.0f, 0.0f });
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

    // Swapchain must be destroyed before the window, since the window destroys the surface
    VulkanContext::getDevice(m_deviceID).freeSwapchain(m_swapchainID);
    m_window.free();
    VulkanContext::free();
}

// This function will configure the octree in the GPU. It will create the necessary buffers and images and update the descriptor set
void Engine::configureOctreeBuffer(Octree& octree, const float scale)
{
    m_octree = &octree;
    VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
    if (octree.isFinished()) 
        octree.packAndFinish();

    // Data transfer
    {
        if (m_octreeBuffer != UINT32_MAX)
        {
            device.freeBuffer(m_octreeBuffer);
            for (const uint32_t& key : m_octreeImages | std::views::keys)
                device.freeImage(key);
            m_octreeImages.clear();
        }

        bool transientConfig = false;
        if (!device.isStagingBufferConfigured())
        {
            transientConfig = true;
            device.configureStagingBuffer(100LL * 1024 * 1024, m_transferQueuePos);
        }
        const VkDeviceSize stagingBufferSize = device.getStagingBufferSize();
        
        bool resized = false;
        VkDeviceSize currentBufferSize = stagingBufferSize;
        // Image upload
        {
            for (const std::string& imagePath : octree.getMaterialTextures())
            {
                int texWidth, texHeight, texChannels;
                stbi_uc* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                const VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;
                const VkExtent3D extent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };

                if (!pixels) {
                    throw std::runtime_error("failed to load texture image " + imagePath); 
                }

                // The staging buffer is used to transfer the image data to the GPU
                // We want to send it in one go, so we need to check if the image is too big for the current staging buffer
                {
                    if (imageSize > currentBufferSize)
                    {
                        device.freeStagingBuffer();
                        device.configureStagingBuffer(imageSize, m_transferQueuePos);
                        currentBufferSize = imageSize;
                        resized = true;
                    }
                    void* stagePtr = device.mapStagingBuffer(imageSize, 0);
                    memcpy(stagePtr, pixels, imageSize);
                    device.unmapStagingBuffer();
                    stbi_image_free(pixels);
                }

                const uint32_t imageID = device.createImage(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, extent, VK_IMAGE_USAGE_TRANSFER_DST_BIT |VK_IMAGE_USAGE_SAMPLED_BIT, 0);

                VulkanImage& image = device.getImage(imageID);
                image.allocateFromFlags({VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false});
                image.transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);
                device.dumpStagingBufferToImage(imageID, extent, {0, 0, 0}, 0);
                image.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0);

                VkSampler sampler = image.createSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
                m_octreeImages.emplace_back(imageID, sampler);
                // We keep track of the memory usage of the images for stats
                m_octreeImagesMemUsage += image.getMemoryRequirements().size;
            }
        }

        if (resized)
        {
            device.freeStagingBuffer();
            device.configureStagingBuffer(stagingBufferSize, m_transferQueuePos);
        }

        // Octree data upload
        // We first calculate the total size of the buffer, then we allocate it and copy the data
        // To calculate the size we need to make sure the material buffer is aligned to the GPU requirements, otherwise data will not be read correctly
        const VkDeviceSize alignment = device.getGPU().getProperties().limits.minStorageBufferOffsetAlignment;
        m_octreeMatPadding = alignment - octree.getByteSize() % alignment;
        const VkDeviceSize bufferSize = octree.getByteSize() + m_octreeMatPadding + octree.getMaterialByteSize();
        m_octreeBuffer = device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        device.getBuffer(m_octreeBuffer).allocateFromFlags({ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false });
        m_octreeBufferSize = device.getBuffer(m_octreeBuffer).getSize();

        // We copy the data in chunks to the staging buffer. If we wanted to send it all at once, we would need to allocate a buffer that is at least as big as the octree data
        // That can be a lot, we can't afford to duplicate the memory usage like that. So we copy it little by little, offseting the pointer each time
        VkDeviceSize offset = 0;
        while (offset < octree.getByteSize())
        {
            const VkDeviceSize nextSize = std::min(stagingBufferSize, octree.getByteSize() - offset);
            void* stagePtr = device.mapStagingBuffer(nextSize, 0);
            if (!octree.isReversed())
                memcpy(stagePtr, static_cast<char*>(octree.getData()) + offset, nextSize);
            else
            {
                const uint32_t offsetUint = static_cast<uint32_t>(offset / sizeof(uint32_t));
                const uint32_t nextSizeUint = static_cast<uint32_t>(nextSize / sizeof(uint32_t));
                for (uint32_t i = offsetUint; i < offsetUint + nextSizeUint; i++)
                    static_cast<uint32_t*>(stagePtr)[i - offsetUint] = octree.getRaw(octree.getSize() - 1 - i);
            }
            device.dumpStagingBuffer(m_octreeBuffer, nextSize, offset, 0);
            offset += nextSize;
        }

        // Material data is copied in one go since it's small
        void* stagePtr = device.mapStagingBuffer(octree.getMaterialByteSize(), 0);
        memcpy(stagePtr, octree.getMaterialData(), octree.getMaterialByteSize());
        device.dumpStagingBuffer(m_octreeBuffer, octree.getMaterialByteSize(), octree.getByteSize() + m_octreeMatPadding, 0);
        
        if (transientConfig)
        {
            device.freeStagingBuffer();
        }
    }

    // Descriptor sets
    if (m_octreeDescrPool != UINT32_MAX)
    {
        device.freeDescriptorPool(m_octreeDescrPool);
    }

    m_octreeDescrPool = device.createDescriptorPool({ 
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(octree.getMaterialTextures().size())}
    }, 2, 0);
    m_octreeDescrSet = device.createDescriptorSet(m_octreeDescrPool, m_octreeDescrSetLayout);

    VkDescriptorBufferInfo bufferInfo[2];
    // The buffer is split in two parts: the octree data and the material data. It is all kept in one buffer for performance
    // but we send it to the GPU as two separate buffers
    bufferInfo[0].buffer = *device.getBuffer(m_octreeBuffer);
    bufferInfo[0].offset = 0;
    bufferInfo[0].range = octree.getByteSize();
    bufferInfo[1].buffer = *device.getBuffer(m_octreeBuffer);
    bufferInfo[1].offset = octree.getByteSize() + m_octreeMatPadding;
    bufferInfo[1].range = VK_WHOLE_SIZE;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets{2};
    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].dstSet = *device.getDescriptorSet(m_octreeDescrSet);
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].dstArrayElement = 0;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[0].descriptorCount = 2;
    writeDescriptorSets[0].pBufferInfo = bufferInfo;

    // The images are sent as an image sampler array
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (const auto& imageData : m_octreeImages)
    {
        const VkImageView view = device.getImage(imageData.first).createImageView(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = view;
        imageInfo.sampler = imageData.second;
        imageInfos.push_back(imageInfo);
    }

    writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[1].dstSet = *device.getDescriptorSet(m_octreeDescrSet);
    writeDescriptorSets[1].dstBinding = 2;
    writeDescriptorSets[1].dstArrayElement = 0;
    writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSets[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
    writeDescriptorSets[1].pImageInfo = imageInfos.data();

    device.updateDescriptorSets(writeDescriptorSets);
    
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

        // Sync
        inFlightFence.wait();
        inFlightFence.reset();

        // Acquire
        const uint32_t nextImage = device.getSwapchain(m_swapchainID).acquireNextImage();

        if (nextImage == UINT32_MAX)
        {
            frameCounter++;
            continue;
        }

        // ImGui
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

        // Record
        recordCommandBuffer(m_framebuffers[nextImage], imguiDrawData);

        // Submit
        graphicsBuffer.submit(graphicsQueue, { {device.getSwapchain(m_swapchainID).getImgSemaphore(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT} }, { m_renderFinishedSemaphoreID }, m_inFlightFenceID);

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        // Present
        device.getSwapchain(m_swapchainID).present(m_presentQueuePos, { m_renderFinishedSemaphoreID });

        frameCounter++;
    }
}

void Engine::createRenderPass()
{
    Logger::pushContext("Create RenderPass");
    VulkanRenderPassBuilder builder{};

    const VkFormat format = VulkanContext::getDevice(m_deviceID).getSwapchain(m_swapchainID).getFormat().format;

    // We just have one color attachment, since the scene is raytraced we don't need depth data. The whole scene is rendered in one draw call
    // Maybe support for several draw calls could be added in the future, in which case we would need a depth attachment, but for now it's not necessary
    const VkAttachmentDescription colorAttachment = VulkanRenderPassBuilder::createAttachment(format,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    builder.addAttachment(colorAttachment);

    // Naturally, we only have one subpass
    builder.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, { {COLOR, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL} }, 0);

    m_renderPassID = VulkanContext::getDevice(m_deviceID).createRenderPass(builder, 0);
    Logger::popContext();
}

uint32_t Engine::createGraphicsPipeline(const uint32_t samplerImageCount, const std::string& fragmentShader, std::vector<VulkanShader::MacroDef> macros)
{
    Logger::pushContext("Create Pipeline");

    VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

    // This function may be called several times to update shaders, so we will want to reuse the descriptor set layout and pipeline layout
    // TODO: We probably want to update image sampler array descriptor to support runtime octree updates
    if (m_octreeDescrSetLayout == UINT32_MAX)
    {
        // octree buffer
        VkDescriptorSetLayoutBinding octreeBinding{};
        octreeBinding.binding = 0;
        octreeBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        octreeBinding.descriptorCount = 1;
        octreeBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // material buffer
        VkDescriptorSetLayoutBinding matBinding{};
        matBinding.binding = 1;
        matBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        matBinding.descriptorCount = 1;
        matBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // texture sampler array
        VkDescriptorSetLayoutBinding texBinding{};
        texBinding.binding = 2;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.descriptorCount = samplerImageCount;
        texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_octreeDescrSetLayout = device.createDescriptorSetLayout({ octreeBinding, matBinding, texBinding }, 0);
    }
    if (m_pipelineLayoutID)
    {
        std::vector<VkPushConstantRange> pushConstants{ 1 };
        pushConstants[0] = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData) };
        m_pipelineLayoutID = device.createPipelineLayout({ m_octreeDescrSetLayout }, pushConstants);
    }

    // Shader creation
    const uint32_t vertexShaderID = device.createShader("shaders/raytracing.vert", VK_SHADER_STAGE_VERTEX_BIT, {});
    // These macros allow easy customization of the shader.
    // For example, we can change the number of textures in the sampler array, which is needed at compilation time
    macros.push_back({"SAMPLER_ARRAY_SIZE", std::to_string(samplerImageCount)});
    macros.push_back({"VOXEL_SIZE", std::to_string(m_voxelSize)});
    macros.push_back({"OCTREE_DEPTH", std::to_string(m_depth)});
    const uint32_t fragmentShaderID = device.createShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, macros);

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VulkanPipelineBuilder builder{ &device };

    builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    builder.setViewportState(1, 1);
    builder.setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f);
    builder.setDepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
    builder.addColorBlendAttachment(colorBlendAttachment);
    builder.setColorBlendState(VK_FALSE, VK_LOGIC_OP_COPY, { 0.0f, 0.0f, 0.0f, 0.0f });
    builder.setDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
    builder.addShaderStage(vertexShaderID);
    builder.addShaderStage(fragmentShaderID);
    const uint32_t pipelineID = device.createPipeline(builder, m_pipelineLayoutID, m_renderPassID, 0);

    device.freeShader(vertexShaderID);
    device.freeShader(fragmentShaderID);

    Logger::popContext();
    return pipelineID;
}

uint32_t Engine::createFramebuffer(const VkImageView colorAttachment, const VkExtent2D newExtent) const
{
    const std::vector<VkImageView> attachments{ colorAttachment };
    return VulkanContext::getDevice(m_deviceID).createFramebuffer({ newExtent.width, newExtent.height, 1 }, VulkanContext::getDevice(m_deviceID).getRenderPass(m_renderPassID), attachments);
}

// https://github.com/ocornut/imgui/blob/docking/examples/example_sdl2_vulkan/main.cpp
void Engine::initImgui() const
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

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

// This function hooks the events from the window to the engine.
// The camera uses signals from the VkPlayground library, they are a basic implementation of the observer pattern
void Engine::setupInputEvents()
{
    m_window.getMouseMovedSignal().connect(&cam, &Camera::mouseMoved);
    m_window.getKeyPressedSignal().connect(&cam, &Camera::keyPressed);
    m_window.getKeyReleasedSignal().connect(&cam, &Camera::keyReleased);
    m_window.getEventsProcessedSignal().connect(&cam, &Camera::updateEvents);
    m_window.getMouseCaptureChangedSignal().connect(&cam, &Camera::setMouseCaptured);

    // Rebuild swapchain resources when the window is resized
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

// Record the commands to the command buffer. This function is called every frame
void Engine::recordCommandBuffer(const uint32_t framebufferID, ImDrawData* main_draw_data)
{
    Logger::pushContext("Command buffer recording");
    const VkExtent2D& extent = VulkanContext::getDevice(m_deviceID).getSwapchain(m_swapchainID).getExtent();

    std::vector<VkClearValue> clearValues{ 2 };
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    const uint32_t layout = VulkanContext::getDevice(m_deviceID).getPipeline(m_pipelineID).getLayout();

    const Camera::Data camData = cam.getData();
    const PushConstantData pushConstants{
        camData.position,
        camData.invPVMatrix,
        m_sunlightDir,
        m_skyColor,
        m_sunColor,
        m_octreeScale,
        m_brightness,
        m_saturation,
        m_contrast,
        m_gamma
    };

    VulkanCommandBuffer& graphicsBuffer = VulkanContext::getDevice(m_deviceID).getCommandBuffer(m_graphicsCmdBufferID, 0);
    graphicsBuffer.reset();
    graphicsBuffer.beginRecording();

    graphicsBuffer.cmdBeginRenderPass(m_renderPassID, framebufferID, extent, clearValues);

    graphicsBuffer.cmdBindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, m_octreeDescrSet);

    if (m_intersectionTest)
        if (m_intersectionTestColor)
            graphicsBuffer.cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_intersectColorPipelineID);
        else
            graphicsBuffer.cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_intersectPipelineID);
    else if (m_noShadows)
        graphicsBuffer.cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_noShadowPipelineID);
    else
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

void Engine::drawImgui()
{
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Metrics");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Separator();
    ImGui::Text("Camera position: (%.3f, %.3f, %.3f)", cam.getPosition().x, cam.getPosition().y, cam.getPosition().z);
    ImGui::Text("Camera direction: (%.3f, %.3f, %.3f)", cam.getDir().x, cam.getDir().y, cam.getDir().z);

    ImGui::End();
    ImGui::Begin("Octree stats");
    if (m_octree->isOctreeLoadedFromFile())
    {
        ImGui::Text("Load time: %.4fs", m_octree->getStats().saveTime);
    }
    else
    {
        ImGui::Text("Construction time: %.4fs", m_octree->getStats().constructionTime);
        ImGui::Text("Save time: %.4fs", m_octree->getStats().saveTime);
    }
    ImGui::Separator();
    ImGui::Text("Total nodes: %u nodes", m_octree->getSize());
    ImGui::Text(" - Voxel nodes: %llu nodes (%.4f%%)", m_octree->getStats().voxels, static_cast<float>(m_octree->getStats().voxels) / static_cast<float>(m_octree->getSize()) * 100.0f);
    ImGui::Text(" - Branch nodes: %llu nodes (%.4f%%)", m_octree->getSize() - m_octree->getStats().voxels, static_cast<float>(m_octree->getSize() - m_octree->getStats().voxels) / static_cast<float>(m_octree->getSize()) * 100.0f);
    ImGui::Text(" - Far nodes: %llu nodes (%.4f%%)", m_octree->getStats().farPtrs, static_cast<float>(m_octree->getStats().farPtrs) / static_cast<float>(m_octree->getSize()) * 100.0f);
    ImGui::Text("Materials: %u", m_octree->getStats().materials);
    ImGui::Text("Textures: %u", static_cast<uint32_t>(m_octree->getMaterialTextures().size()));
    ImGui::Separator();
    ImGui::Text("Depth: %d", m_octree->getDepth());
    ImGui::Text("Density: %.4f%%", static_cast<float>(m_octree->getStats().voxels) / static_cast<float>(std::pow(8, m_octree->getDepth())) * 100.0f);
    ImGui::Separator();
    ImGui::Text("GPU Memory usage: %s", VulkanMemoryAllocator::compactBytes(m_octreeImagesMemUsage + m_octreeBufferSize).c_str());
    ImGui::Text(" - GPU Memory usage (octree): %s", VulkanMemoryAllocator::compactBytes(m_octreeBufferSize).c_str());
    ImGui::Text(" - GPU Memory usage (images): %s", VulkanMemoryAllocator::compactBytes(m_octreeImagesMemUsage).c_str());
    ImGui::Text("CPU Memory usage: %s", VulkanMemoryAllocator::compactBytes(m_octree->getByteSize()).c_str());
    ImGui::End();

    ImGui::Begin("Settings");
    ImGui::InputFloat("Scale", &m_octreeScale, 0.1f, 1.0f);
    ImGui::Separator();
    ImGui::SliderFloat("Sun Latitude", &m_sunRotationLat, -180.0f, 180.0f);
    ImGui::SliderFloat("Sun Altitude", &m_sunRotationAlt, -180.0f, 180.0f);
    const float latRads = glm::radians(m_sunRotationLat);
    const float altRads = glm::radians(m_sunRotationAlt + 90);
    m_sunlightDir.x = glm::cos(altRads) * glm::cos(latRads);
    m_sunlightDir.y = glm::sin(altRads);
    m_sunlightDir.z = glm::cos(altRads) * glm::sin(latRads);
    ImGui::ColorEdit3("Sun color", &m_sunColor.x);
    ImGui::ColorEdit3("Sky color", &m_skyColor.x);
    ImGui::Separator();
    ImGui::DragFloat("Brightness", &m_brightness, 0.001f, -1, 1);
	ImGui::DragFloat("Saturation", &m_saturation, 0.001f, -10, 10);
	ImGui::DragFloat("Contrast", &m_contrast, 0.001f, 0, 1);
	ImGui::DragFloat("Gamma", &m_gamma, 0.001f, 0, 4);
    ImGui::Separator();
    if (ImGui::Button("Reload shaders"))
        updatePipelines();
    if (!m_intersectionTest)
        ImGui::Checkbox("No shadows", &m_noShadows);
    ImGui::Checkbox("Intersection test", &m_intersectionTest);
    if (m_intersectionTest)
        ImGui::Checkbox("Enable color intersection", &m_intersectionTestColor);
    ImGui::End();
}

void Engine::updatePipelines()
{
    VulkanDevice& device = VulkanContext::getDevice(m_deviceID);

    try
    {
        uint32_t oldPipeline = m_pipelineID;
        m_pipelineID = createGraphicsPipeline(m_samplerImageCount, "shaders/raytracing.frag", {});
        if (oldPipeline != UINT32_MAX)
            device.freePipeline(oldPipeline);
        oldPipeline = m_noShadowPipelineID;
        m_noShadowPipelineID = createGraphicsPipeline(m_samplerImageCount, "shaders/raytracing.frag", {{"NO_SHADOW", "true"}});
        if (oldPipeline != UINT32_MAX)
            device.freePipeline(oldPipeline);
        oldPipeline = m_intersectPipelineID;
        m_intersectPipelineID = createGraphicsPipeline(m_samplerImageCount, "shaders/raytracing.frag", {{"INTERSECTION_TEST", "true"}});
        if (oldPipeline != UINT32_MAX)
            device.freePipeline(oldPipeline);
        oldPipeline = m_intersectColorPipelineID;
        m_intersectColorPipelineID = createGraphicsPipeline(m_samplerImageCount, "shaders/raytracing.frag", {{"INTERSECTION_TEST", "true"}, {"INTERSECTION_COLOR", "true"}});
        if (oldPipeline != UINT32_MAX)
            device.freePipeline(oldPipeline);
    }
    catch (const std::exception& e)
    {
        Logger::print(std::string("Failed to reload shaders: ") + e.what(), Logger::ERR);
    }
    
}
