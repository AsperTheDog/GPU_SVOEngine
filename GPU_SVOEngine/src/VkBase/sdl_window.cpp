#include "VkBase/sdl_window.hpp"

#include <iostream>
#include <stdexcept>
#include <SDL2/SDL.h>

#include "logger.hpp"
#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"
#include "VkBase/vulkan_sync.hpp"

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
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && event.window.data1 > 0 && event.window.data2 > 0)
				rebuildSwapchain(WindowSize{event.window.data1, event.window.data2}.toExtent2D());
			break;
		}
	}
}

void SDLWindow::createSurface()
{
	if (m_surface != nullptr) 
		throw std::runtime_error("Surface already created");

	if (SDL_Vulkan_CreateSurface(m_SDLHandle, VulkanContext::m_vkHandle, &m_surface) == SDL_FALSE)
		throw std::runtime_error("failed to create SDLHandle surface!");
}

void SDLWindow::createSwapchain(const uint32_t deviceID, const VkSurfaceFormatKHR desiredFormat)
{
	if (m_surface == nullptr)
		throw std::runtime_error("Surface not created");

	if (m_swapchain.swapchain != nullptr)
	{
		freeSwapchain();
		Logger::print("Swapchain already created, freed old swapchain");
	}

	const VkSurfaceFormatKHR selectedFormat = VulkanContext::getDevice(deviceID).getGPU().getClosestFormat(*this, desiredFormat);
	_createSwapchain(deviceID, getSize().toExtent2D(), selectedFormat);
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

bool SDLWindow::getAndResetSwapchainRebuildFlag()
{
	const bool ret = m_swapchain.rebuilt;
	m_swapchain.rebuilt = false;
	return ret;
}

VkImageView SDLWindow::getImageView(const uint32_t index) const
{
	return m_swapchain.imageViews[index];
}

uint32_t SDLWindow::getImageCount() const
{
	return static_cast<uint32_t>(m_swapchain.images.size());
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
		Logger::print("Freed swapchain");
	}
}

void SDLWindow::rebuildSwapchain(const VkExtent2D newExtent)
{
	if (m_swapchain.swapchain == nullptr || VulkanContext::getDevice(m_deviceID).m_vkHandle == nullptr)
		return;

	m_swapchain.rebuilt = true;

	Logger::pushContext("Swapchain rebuild");
	freeSwapchain();
	_createSwapchain(m_deviceID, newExtent, m_swapchain.format);
	Logger::popContext();
}

void SDLWindow::_createSwapchain(const uint32_t deviceID, const VkExtent2D size, const VkSurfaceFormatKHR format)
{
	m_deviceID = deviceID;
	const VulkanDevice& device = VulkanContext::getDevice(m_deviceID);
	const VkSurfaceCapabilitiesKHR capabilities = device.m_physicalDevice.getCapabilities(*this);

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
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device.m_vkHandle, &createInfo, nullptr, &m_swapchain.swapchain) != VK_SUCCESS)
		throw std::runtime_error("failed to create swap chain!");
	Logger::print("Created swapchain");

	m_swapchain.format = format;
	m_swapchain.extent = createInfo.imageExtent;

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
		Logger::print("Created image view");
    }
	Logger::popContext();
}
