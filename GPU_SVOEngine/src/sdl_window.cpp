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
    return { width, height };
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

    m_keyPressed.connect([this](const uint32_t key)
        {
            if (key == SDLK_q)
                toggleMouseCapture();
        });
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
    return { width, height };
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
                m_resizeSignal.emit(WindowSize {event.window.data1, event.window.data2}.toExtent2D());
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
    m_mouseCaptureChanged.emit(m_mouseCaptured);
}

void SDLWindow::createSurface(const VkInstance instance)
{
    if (m_surface != nullptr)
        throw std::runtime_error("Surface already created");

    if (SDL_Vulkan_CreateSurface(m_SDLHandle, instance, &m_surface) == SDL_FALSE)
        throw std::runtime_error("failed to create SDLHandle surface!");
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

void SDLWindow::frameImgui() const
{
    ImGui_ImplSDL2_NewFrame();
}

Signal<VkExtent2D>& SDLWindow::getResizedSignal()
{
    return m_resizeSignal;
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

Signal<bool>& SDLWindow::getMouseCaptureChangedSignal()
{
    return m_mouseCaptureChanged;
}
