#pragma once
#include <SDL2/SDL_vulkan.h>
#include <string_view>
#include <vector>
#include <SDL2/SDL_keycode.h>
#include <vulkan/vulkan_core.h>

#include "utils/signal.hpp"

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

	void initImgui() const;

	[[nodiscard]] bool shouldClose() const;
	[[nodiscard]] std::vector<const char*> getRequiredVulkanExtensions() const;
	[[nodiscard]] WindowSize getSize() const;

	[[nodiscard]] VkSurfaceFormatKHR getSwapchainImageFormat() const;
	[[nodiscard]] VkExtent2D getSwapchainExtent() const;

	void pollEvents();
	void toggleMouseCapture();

	void createSurface();
	void createSwapchain(const uint32_t deviceID, const VkSurfaceFormatKHR desiredFormat, const VkPresentModeKHR presentMode);

	[[nodiscard]] uint32_t acquireNextImage(uint32_t semaphoreID, const VulkanFence* fence = nullptr) const;
	[[nodiscard]] VkImageView getImageView(uint32_t index) const;
	[[nodiscard]] uint32_t getImageCount() const;
	[[nodiscard]] uint32_t getMinImageCount() const;

	SDL_Window* operator*() const;
	[[nodiscard]] VkSurfaceKHR getSurface() const;

	void free();
	void shutdownImgui() const;

	void present(const VulkanQueue& queue, uint32_t imageIndex, uint32_t waitSemaphore = UINT32_MAX) const;
	void frameImgui() const;

	[[nodiscard]] Signal<VkExtent2D>& getSwapchainRebuiltSignal();
	[[nodiscard]] Signal<int32_t, int32_t>& getMouseMovedSignal();
	[[nodiscard]] Signal<uint32_t>& getKeyPressedSignal();
	[[nodiscard]] Signal<uint32_t>& getKeyReleasedSignal();
	[[nodiscard]] Signal<float>& getEventsProcessedSignal();

private:
	struct Swapchain
	{
		VkSwapchainKHR swapchain = nullptr;
		VkExtent2D extent;
		VkSurfaceFormatKHR format;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		uint32_t minImageCount = 0;
        VkPresentModeKHR presentMode;
		bool rebuilt = false;
	};

	SDL_Window* m_SDLHandle = nullptr;
	VkSurfaceKHR m_surface = nullptr;
	Swapchain m_swapchain{};

	uint32_t m_deviceID = UINT32_MAX;

	// Signals
	Signal<VkExtent2D> m_swapchainRebuilt;
	Signal<int32_t, int32_t> m_mouseMoved;
	Signal<uint32_t> m_keyPressed;
	Signal<uint32_t> m_keyReleased;
	Signal<float> m_eventsProcessed;

	float prevDt = 0.f;
	float dt = 0.f;

	bool m_mouseCaptured = false;

	void freeSwapchain();
	void rebuildSwapchain(VkExtent2D newExtent);

	void _createSwapchain(const uint32_t deviceID, const VkExtent2D size, const VkSurfaceFormatKHR format, VkPresentModeKHR presentMode);

	friend class Surface;
	friend class VulkanGPU;
};

