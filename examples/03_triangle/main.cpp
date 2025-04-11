/*
 * @author: Avidel
 * @LastEditors: Avidel
 */

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_stdinc.h"
#define SDL_MAIN_USE_CALLBACKS
#include "SDL3/SDL_init.h"
#include "Utils/vulkan_util.hpp"
#include "spdlog/spdlog.h"
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan_core.h>

constexpr Uint32 window_height = 600;
constexpr Uint32 window_width = 800;

SDL_AppResult SDL_AppInit(void** /*appstate*/, int /*argc*/, char* /*argv*/[]) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%s:%# %!] [%l] %v");
    VulkanContextManager* sdl_vk_instance = VulkanContextManager::getInstance();

    std::unique_ptr<SDLContext> sdl_context = std::make_unique<SDLContext>();
    SDL_Init(SDL_INIT_VIDEO);
    auto* window = SDL_CreateWindow("Dynamic Triangle", window_width,
                                    window_height, SDL_WINDOW_VULKAN);
    sdl_context->setWindow(window);
    sdl_vk_instance->initVulkan(std::move(sdl_context));
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* /*appstate*/, SDL_AppResult /*result*/) {
    auto* sdl_vk_instance = VulkanContextManager::getInstance();
    SDL_Quit();
}

SDL_AppResult SDL_AppEvent(void* /*appstate*/, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* /*appstate*/) {
    return SDL_APP_CONTINUE;
}
