#include "sdl_window.hpp"

#include <iostream>
#include <stdexcept>
#include <SDL2/SDL.h>

#include "utils/logger.hpp"
#include "backends/imgui_impl_sdl2.h"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"
#include "vulkan_sync.hpp"

VkExtent2D SDLWindow::WindowSize::toExtent2D() const
{
	return {width, height};
}

SDLWindow::WindowSize::WindowSize(const uint32_t width, const uint32_t height)
	: width(width), height(height)
{

}

SDLWindow::WindowSize::WindowSize(const Sint32 width, const Sint32 height)
	: width(static_cast<uint32_t>(width)), height(static_cast<uint32_t>(height))
{

}

SDLWindow::SDLWindow(const std::string_view name, const int width, const int height, const int top, const int left, const uint32_t flags)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	m_SDLHandle = SDL_CreateWindow(name.data(), top, left, width, height, flags | SDL_WINDOW_VULKAN);
	toggleMouseCapture();
}

void SDLWindow::initImgui() const
{
	ImGui_ImplSDL2_InitForVulkan(m_SDLHandle);
}

bool SDLWindow::shouldClose() const
{
	return SDL_QuitRequested();
}

std::vector<const char*> SDLWindow::getRequiredVulkanExtensions() const
{
	uint32_t extensionCount;
	SDL_Vulkan_GetInstanceExtensions(m_SDLHandle, &extensionCount, nullptr);

	std::vector<const char*> extensions(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(m_SDLHandle, &extensionCount, extensions.data());

	return extensions;
}

SDLWindow::WindowSize SDLWindow::getSize() const
{
	Sint32 width, height;
	SDL_GetWindowSize(m_SDLHandle, &width, &height);
	return {width, height};
}

VkSurfaceFormatKHR SDLWindow::getSwapchainImageFormat() const
{
	return m_swapchain.format;
}

VkExtent2D SDLWindow::getSwapchainExtent() const
{
	return m_swapchain.extent;
}

void SDLWindow::pollEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && event.window.data1 > 0 && event.window.data2 > 0)
				rebuildSwapchain(WindowSize{event.window.data1, event.window.data2}.toExtent2D());
			break;
		case SDL_MOUSEMOTION:
			m_mouseMoved.emit(event.motion.xrel, event.motion.yrel);
			break;
		case SDL_KEYDOWN:
			m_keyPressed.emit(event.key.keysym.sym);
			break;
		case SDL_KEYUP:
			m_keyReleased.emit(event.key.keysym.sym);
			break;
		}
	}
	const uint64_t now = SDL_GetTicks64();
	dt = (static_cast<float>(now) - prevDt) * 0.001f;
	prevDt = static_cast<float>(now);
	m_eventsProcessed.emit(dt);
}

void SDLWindow::toggleMouseCapture()
{
	m_mouseCaptured = !m_mouseCaptured;
	if (m_mouseCaptured)
		SDL_SetRelativeMouseMode(SDL_TRUE);
	else
		SDL_SetRelativeMouseMode(SDL_FALSE);
}

void SDLWindow::createSurface()
{
	if (m_surface != nullptr) 
		throw std::runtime_error("Surface already created");

	if (SDL_Vulkan_CreateSurface(m_SDLHandle, VulkanContext::m_vkHandle, &m_surface) == SDL_FALSE)
		throw std::runtime_error("failed to create SDLHandle surface!");
}

void SDLWindow::createSwapchain(const uint32_t deviceID, const VkSurfaceFormatKHR desiredFormat, const VkPresentModeKHR presentMode)
{
	if (m_surface == nullptr)
		throw std::runtime_error("Surface not created");

	if (m_swapchain.swapchain != nullptr)
	{
		freeSwapchain();
		Logger::print("Swapchain already created, freed old swapchain", Logger::LevelBits::WARN);
	}

	const VkSurfaceFormatKHR selectedFormat = VulkanContext::getDevice(deviceID).getGPU().getClosestFormat(m_surface, desiredFormat);
	_createSwapchain(deviceID, getSize().toExtent2D(), selectedFormat, presentMode);
}

uint32_t SDLWindow::acquireNextImage(const uint32_t semaphoreID, const VulkanFence* fence) const
{
	if (m_swapchain.swapchain == nullptr)
		throw std::runtime_error("Swapchain not created");

	uint32_t imageIndex;
	VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	const VulkanSemaphore& semaphore = device.getSemaphore(semaphoreID);
	const VkResult result = vkAcquireNextImageKHR(device.m_vkHandle, m_swapchain.swapchain, UINT64_MAX, semaphore.m_vkHandle, fence != nullptr ? fence->m_vkHandle : nullptr, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return UINT32_MAX;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	return imageIndex;
}

VkImageView SDLWindow::getImageView(const uint32_t index) const
{
	return m_swapchain.imageViews[index];
}

uint32_t SDLWindow::getImageCount() const
{
	return static_cast<uint32_t>(m_swapchain.images.size());
}

uint32_t SDLWindow::getMinImageCount() const
{
	return m_swapchain.minImageCount;
}

SDL_Window* SDLWindow::operator*() const
{
	return m_SDLHandle;
}

VkSurfaceKHR SDLWindow::getSurface() const
{
	return m_surface;
}

void SDLWindow::free()
{
	freeSwapchain();
	m_swapchain = {};

	if (m_surface != nullptr)
	{
		vkDestroySurfaceKHR(VulkanContext::m_vkHandle, m_surface, nullptr);
		m_surface = nullptr;
	}

	SDL_DestroyWindow(m_SDLHandle);
	SDL_Quit();
	m_SDLHandle = nullptr;
}

void SDLWindow::shutdownImgui() const
{
	ImGui_ImplSDL2_Shutdown();
}

void SDLWindow::present(const VulkanQueue& queue, const uint32_t imageIndex, const uint32_t waitSemaphore) const
{
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = waitSemaphore != UINT32_MAX ? 1 : 0;
	presentInfo.pWaitSemaphores = &VulkanContext::getDevice(m_deviceID).getSemaphore(waitSemaphore).m_vkHandle;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult result = vkQueuePresentKHR(queue.m_vkHandle, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) return;
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void SDLWindow::frameImgui() const
{
	ImGui_ImplSDL2_NewFrame();
}

Signal<VkExtent2D>& SDLWindow::getSwapchainRebuiltSignal()
{
	return m_swapchainRebuilt;
}

Signal<int32_t, int32_t>& SDLWindow::getMouseMovedSignal()
{
	return m_mouseMoved;
}

Signal<uint32_t>& SDLWindow::getKeyPressedSignal()
{
	return m_keyPressed;
}

Signal<uint32_t>& SDLWindow::getKeyReleasedSignal()
{
	return m_keyReleased;
}

Signal<float>& SDLWindow::getEventsProcessedSignal()
{
	return m_eventsProcessed;
}

void SDLWindow::freeSwapchain()
{
	const VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	if (m_swapchain.swapchain != nullptr && device.m_vkHandle != nullptr)
	{
		for (const auto& imageView : m_swapchain.imageViews)
		{
			vkDestroyImageView(device.m_vkHandle, imageView, nullptr);
		}
		m_swapchain.imageViews.clear();
		m_swapchain.images.clear();
		vkDestroySwapchainKHR(device.m_vkHandle, m_swapchain.swapchain, nullptr);
		m_swapchain.swapchain = nullptr;
		Logger::print("Freed swapchain", Logger::LevelBits::INFO);
	}
}

void SDLWindow::rebuildSwapchain(const VkExtent2D newExtent)
{
	if (m_swapchain.swapchain == nullptr || VulkanContext::getDevice(m_deviceID).m_vkHandle == nullptr)
		return;

	m_swapchain.rebuilt = true;

	Logger::pushContext("Swapchain rebuild");
	freeSwapchain();
	_createSwapchain(m_deviceID, newExtent, m_swapchain.format, m_swapchain.presentMode);

    m_swapchainRebuilt.emit(newExtent);

	Logger::popContext();
}

void SDLWindow::_createSwapchain(const uint32_t deviceID, const VkExtent2D size, const VkSurfaceFormatKHR format, const VkPresentModeKHR presentMode)
{
	m_deviceID = deviceID;
	const VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	const VkSurfaceCapabilitiesKHR capabilities = device.m_physicalDevice.getCapabilities(m_surface);

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && createInfo.minImageCount > capabilities.maxImageCount)
    	createInfo.minImageCount = capabilities.maxImageCount;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent = size;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device.m_vkHandle, &createInfo, nullptr, &m_swapchain.swapchain) != VK_SUCCESS)
		throw std::runtime_error("failed to create swap chain!");
	Logger::print("Created swapchain", Logger::LevelBits::INFO);

	m_swapchain.format = format;
	m_swapchain.extent = createInfo.imageExtent;
	m_swapchain.minImageCount = createInfo.minImageCount;
    m_swapchain.presentMode = presentMode;

	// Get images
	uint32_t imageCount;
	vkGetSwapchainImagesKHR(device.m_vkHandle, m_swapchain.swapchain, &imageCount, nullptr);
    m_swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device.m_vkHandle, m_swapchain.swapchain, &imageCount, m_swapchain.images.data());

	// Create image views
	Logger::pushContext("Swapchain Image Views");
	m_swapchain.imageViews.resize(m_swapchain.images.size());
    for (uint32_t i = 0; i < m_swapchain.images.size(); i++) {
		VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchain.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.m_vkHandle, &viewInfo, nullptr, &m_swapchain.imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swapchain image view!");
        }
		Logger::print("Created image view", Logger::LevelBits::INFO);
    }
	Logger::popContext();
}
