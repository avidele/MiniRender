/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#pragma once
#include "SDL3/SDL_video.h"
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#define EnableDebug 1
#if defined(__APPLE__)
#define VKB_ENABLE_PORTABILITY 1
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif
struct Vertex {
    float pos[2];
    float color[3];
};

struct SDLWindowDeleter {
    void operator()(SDL_Window* window) const {
        if (window) {
            SDL_DestroyWindow(window);
        }
    }
};

class SDLContext {
    std::unique_ptr<SDL_Window, SDLWindowDeleter> window;

public:
    SDLContext() : window(nullptr) {}

    SDL_Window* getWindowPtr() const { return window.get(); }

    std::unique_ptr<SDL_Window, SDLWindowDeleter> getWindow() {
        return std::move(window);
    }

    void setWindow(SDL_Window* win) { window.reset(win); }
};

class VulkanContextManager {
public:
    static VulkanContextManager* getInstance() {
        static VulkanContextManager instance;
        return &instance;
    }

    Vertex vertices[3] = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 红色顶点
        { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 绿色顶点
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}  // 蓝色顶点
    };

    void initVulkan(std::unique_ptr<SDLContext> sdl_context);
    void initDevice();
    void createSurface(std::unique_ptr<SDLContext> sdl_context);
    void initSwapChain();
    void clearVulkan();

#if EnableDebug
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData);

    void setupDebugMessenger();
#endif

private:
    VulkanContextManager() = default;
    ~VulkanContextManager() = default;
#if EnableDebug
    VkDebugUtilsMessengerEXT debug_messenger{VK_NULL_HANDLE};
#endif

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSwapchainKHR swap_chain;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
};
