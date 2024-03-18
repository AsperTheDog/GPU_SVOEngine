#pragma once
#include <SDL2/SDL_vulkan.h>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

class VulkanFence;
class VulkanDevice;
class VulkanContext;
class VulkanQueue;

class SDLWindow
{
public:
	struct WindowSize
	{
		uint32_t width;
		uint32_t height;

		[[nodiscard]] VkExtent2D toExtent2D() const;
		WindowSize(uint32_t width, uint32_t height);
		WindowSize(Sint32 width, Sint32 height);
	};

	SDLWindow() = default;
	SDLWindow(std::string_view name, int width, int height, int top = SDL_WINDOWPOS_CENTERED, int left = SDL_WINDOWPOS_CENTERED, uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE);

	[[nodiscard]] bool shouldClose() const;
	[[nodiscard]] std::vector<const char*> getRequiredVulkanExtensions() const;
	[[nodiscard]] WindowSize getSize() const;

	[[nodiscard]] VkSurfaceFormatKHR getSwapchainImageFormat() const;
	[[nodiscard]] VkExtent2D getSwapchainExtent() const;

	void pollEvents();

	void createSurface();
	void createSwapchain(uint32_t deviceID, VkSurfaceFormatKHR desiredFormat);

	[[nodiscard]] uint32_t acquireNextImage(uint32_t semaphoreID, const VulkanFence* fence = nullptr) const;
	[[nodiscard]] bool getAndResetSwapchainRebuildFlag();
	[[nodiscard]] VkImageView getImageView(uint32_t index) const;
	[[nodiscard]] uint32_t getImageCount() const;

	SDL_Window* operator*() const;
	[[nodiscard]] VkSurfaceKHR getSurface() const;

	void free();
	void present(const VulkanQueue& queue, uint32_t imageIndex, uint32_t waitSemaphore = UINT32_MAX) const;

private:
	struct Swapchain
	{
		VkSwapchainKHR swapchain = nullptr;
		VkExtent2D extent;
		VkSurfaceFormatKHR format;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		bool rebuilt = false;
	};

	SDL_Window* m_SDLHandle = nullptr;
	VkSurfaceKHR m_surface = nullptr;
	Swapchain m_swapchain{};

	uint32_t m_deviceID = UINT32_MAX;

	void freeSwapchain();
	void rebuildSwapchain(VkExtent2D newExtent);

	void _createSwapchain(uint32_t deviceID, VkExtent2D size, VkSurfaceFormatKHR format);

	friend class Surface;
	friend class VulkanGPU;
};

