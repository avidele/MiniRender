#pragma once
#include "SDL3/SDL_video.h"
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#define EnableDebug 1
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

    std::unique_ptr<SDL_Window, SDLWindowDeleter> getWindow() {
        return std::move(window);
    }

    void setWindow(SDL_Window* win) {
        window.reset(win);
    }
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
    void clearVulkan();

private:
    VulkanContextManager() = default;
    ~VulkanContextManager() = default;

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

