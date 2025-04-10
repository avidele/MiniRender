/*
 * @Author: Avidel_Arch 1215935448@qq.com
 * @Date: 2025-03-23 23:58:13
 * @LastEditors: Avidel
 * @LastEditTime: 2025-04-10 23:26:07
 * @FilePath: /MiniRender/examples/03_triangle/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AEN
 */
#include "SDL3/SDL_events.h"
#include <vector>
#define SDL_MAIN_USE_CALLBACKS
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <vulkan/vulkan_core.h>

#define EnableDebug 1

struct Vertex {
    float pos[2];
    float color[3];
};

class SDLVkInstance {
public:
    static SDLVkInstance* getInstance() {
        static SDLVkInstance instance;
        return &instance;
    }

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

    SDL_Window* window;

    Vertex vertices[3] = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 红色顶点
        { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 绿色顶点
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}  // 蓝色顶点
    };

private:
    SDLVkInstance() = default;
    ~SDLVkInstance() = default;
};

void InitVulkan() {
    auto* sdl_vk_instance = SDLVkInstance::getInstance();
    auto *window = SDLVkInstance::getInstance()->window;
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dynamic Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t extension_count = 0;
    SDL_Vulkan_GetInstanceExtensions(&extension_count);
#if EnableDebug
    std::cout << "Vulkan extension count: " << extension_count << std::endl;
    for (uint32_t i = 0; i < extension_count; ++i) {
        const char* extension_name =
            SDL_Vulkan_GetInstanceExtensions(&extension_count)[i];
        std::cout << "Vulkan extension: " << extension_name << std::endl;
    }
#endif

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames =
            SDL_Vulkan_GetInstanceExtensions(&extension_count),
    };

    if (vkCreateInstance(&create_info, nullptr, &sdl_vk_instance->instance) !=
        VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (!SDL_Vulkan_CreateSurface(window, sdl_vk_instance->instance, nullptr,
                                  &sdl_vk_instance->surface)) {
        std::cerr << "Failed to create Vulkan surface!" << std::endl;
        exit(EXIT_FAILURE);
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(sdl_vk_instance->instance, &device_count,
                               nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(sdl_vk_instance->instance, &device_count,
                               devices.data());
#if EnableDebug
    std::cout << "Vulkan device count: " << device_count << std::endl;
    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);
        std::cout << "Vulkan device: " << device_properties.deviceName
                  << std::endl;
    }
#endif
    sdl_vk_instance->physical_device = devices[0];

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // 正确的类型
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
    };

    // 在获取设备队列之后，获取内存需求之前添加这段代码
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(sdl_vk_instance->vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    if (vkCreateBuffer(sdl_vk_instance->device, &buffer_info, nullptr,
                       &sdl_vk_instance->vertex_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (vkCreateDevice(sdl_vk_instance->physical_device, &device_create_info,
                       nullptr, &sdl_vk_instance->device) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan device!" << std::endl;
        exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(sdl_vk_instance->device, 0, 0,
                     &sdl_vk_instance->graphics_queue);

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(sdl_vk_instance->device,
                                  sdl_vk_instance->vertex_buffer,
                                  &mem_requirements);

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = 0,
    };

    if (vkAllocateMemory(sdl_vk_instance->device, &alloc_info, nullptr,
                         &sdl_vk_instance->vertex_buffer_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory!" << std::endl;
        exit(EXIT_FAILURE);
    }

    vkBindBufferMemory(sdl_vk_instance->device, sdl_vk_instance->vertex_buffer,
                       sdl_vk_instance->vertex_buffer_memory, 0);
}

void updateVertexBuffer(int mouseX, int mouseY, int windowWidth,
                        int windowHeight) {
    float x = (mouseX / static_cast<float>(windowWidth)) * 2.0f - 1.0f;
    float y = 1.0f - (mouseY / static_cast<float>(windowHeight)) * 2.0f;
    SDLVkInstance::getInstance()->vertices[2].pos[0] = x;
    SDLVkInstance::getInstance()->vertices[2].pos[1] = y;

    void* data;
    vkMapMemory(SDLVkInstance::getInstance()->device,
                SDLVkInstance::getInstance()->vertex_buffer_memory, 0,
                sizeof(SDLVkInstance::getInstance()->vertices), 0, &data);
    memcpy(data, SDLVkInstance::getInstance()->vertices,
           sizeof(SDLVkInstance::getInstance()->vertices));
    vkUnmapMemory(SDLVkInstance::getInstance()->device,
                  SDLVkInstance::getInstance()->vertex_buffer_memory);
}

SDL_AppResult SDL_AppInit(void** /*appstate*/, int /*argc*/, char* /*argv*/[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDLVkInstance::getInstance()->window =
        SDL_CreateWindow("Dynamic Triangle", 800, 600, SDL_WINDOW_VULKAN);
    InitVulkan();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* /*appstate*/, SDL_AppResult  /*result*/) {
    auto* sdl_vk_instance = SDLVkInstance::getInstance();
    vkDestroyBuffer(sdl_vk_instance->device, sdl_vk_instance->vertex_buffer,
                    nullptr);
    vkFreeMemory(sdl_vk_instance->device, sdl_vk_instance->vertex_buffer_memory,
                 nullptr);
    vkDestroyDevice(sdl_vk_instance->device, nullptr);
    vkDestroySurfaceKHR(sdl_vk_instance->instance, sdl_vk_instance->surface,
                        nullptr);
    vkDestroyInstance(sdl_vk_instance->instance, nullptr);
    SDL_DestroyWindow(sdl_vk_instance->window);
    SDL_Quit();
}

SDL_AppResult SDL_AppEvent(void* /*appstate*/, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        int width, height;
        SDL_GetWindowSize(SDLVkInstance::getInstance()->window, &width,
                          &height);
        updateVertexBuffer(event->motion.x, event->motion.y, width, height);
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void*  /*appstate*/) {
    return SDL_APP_CONTINUE;
}
