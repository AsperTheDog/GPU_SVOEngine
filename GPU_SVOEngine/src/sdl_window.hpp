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


	[[nodiscard]] bool shouldClose() const;
	[[nodiscard]] std::vector<const char*> getRequiredVulkanExtensions() const;
	[[nodiscard]] WindowSize getSize() const;

	void pollEvents();
	void toggleMouseCapture();

	void createSurface(VkInstance instance);

	SDL_Window* operator*() const;
	[[nodiscard]] VkSurfaceKHR getSurface() const;

	void free();

	void initImgui() const;
	void frameImgui() const;
	void shutdownImgui() const;

	[[nodiscard]] Signal<VkExtent2D>& getResizedSignal();
	[[nodiscard]] Signal<int32_t, int32_t>& getMouseMovedSignal();
	[[nodiscard]] Signal<uint32_t>& getKeyPressedSignal();
	[[nodiscard]] Signal<uint32_t>& getKeyReleasedSignal();
	[[nodiscard]] Signal<float>& getEventsProcessedSignal();
    [[nodiscard]] Signal<bool>& getMouseCaptureChangedSignal();

private:

	SDL_Window* m_SDLHandle = nullptr;
	VkSurfaceKHR m_surface = nullptr;

    VkInstance m_instance = nullptr;

	// Signals
	Signal<VkExtent2D> m_resizeSignal; //WindowSize
	Signal<int32_t, int32_t> m_mouseMoved; // relX, relY, isMouseCaptured
	Signal<uint32_t> m_keyPressed; // key, isMouseCaptured
	Signal<uint32_t> m_keyReleased; // key, isMouseCaptured
	Signal<float> m_eventsProcessed; // delta
    Signal<bool> m_mouseCaptureChanged; // isMouseCaptured

	float prevDt = 0.f;
	float dt = 0.f;

	bool m_mouseCaptured = false;

	friend class Surface;
	friend class VulkanGPU;
};

