/*
 * @Author: Avidel_Arch 1215935448@qq.com
 * @Date: 2025-03-23 23:58:13
 * @LastEditors: nolanyzhang
 * @LastEditTime: 2025-03-27 22:37:08
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
    static SDLVkInstance* GetInstance() {
        static SDLVkInstance instance;
        return &instance;
    }

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapChain;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

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
    auto sdlVkInstance = SDLVkInstance::GetInstance();
    auto window = SDLVkInstance::GetInstance()->window;
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dynamic Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&extensionCount);
#if EnableDebug
    std::cout << "Vulkan extension count: " << extensionCount << std::endl;
    for (uint32_t i = 0; i < extensionCount; ++i) {
        const char* extensionName =
            SDL_Vulkan_GetInstanceExtensions(&extensionCount)[i];
        std::cout << "Vulkan extension: " << extensionName << std::endl;
    }
#endif

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames =
            SDL_Vulkan_GetInstanceExtensions(&extensionCount),
    };

    if (vkCreateInstance(&createInfo, nullptr, &sdlVkInstance->instance) !=
        VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (!SDL_Vulkan_CreateSurface(window, sdlVkInstance->instance, nullptr,
                                  &sdlVkInstance->surface)) {
        std::cerr << "Failed to create Vulkan surface!" << std::endl;
        exit(EXIT_FAILURE);
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(sdlVkInstance->instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(sdlVkInstance->instance, &deviceCount,
                               devices.data());
#if EnableDebug
    std::cout << "Vulkan device count: " << deviceCount << std::endl;
    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
        std::cout << "Vulkan device: " << deviceProperties.deviceName
                  << std::endl;
    }
#endif
    sdlVkInstance->physicalDevice = devices[0];

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // 正确的类型
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
    };

    // 在获取设备队列之后，获取内存需求之前添加这段代码
    VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .size = sizeof(sdlVkInstance->vertices),
                                  .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    if (vkCreateBuffer(sdlVkInstance->device, &bufferInfo, nullptr,
                       &sdlVkInstance->vertexBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (vkCreateDevice(sdlVkInstance->physicalDevice, &deviceCreateInfo,
                       nullptr, &sdlVkInstance->device) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan device!" << std::endl;
        exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(sdlVkInstance->device, 0, 0,
                     &sdlVkInstance->graphicsQueue);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(
        sdlVkInstance->device, sdlVkInstance->vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = 0,
    };

    if (vkAllocateMemory(sdlVkInstance->device, &allocInfo, nullptr,
                         &sdlVkInstance->vertexBufferMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory!" << std::endl;
        exit(EXIT_FAILURE);
    }

    vkBindBufferMemory(sdlVkInstance->device, sdlVkInstance->vertexBuffer,
                       sdlVkInstance->vertexBufferMemory, 0);
}

void updateVertexBuffer(int mouseX, int mouseY, int windowWidth,
                        int windowHeight) {
    float x = (mouseX / (float)windowWidth) * 2.0f - 1.0f;
    float y = 1.0f - (mouseY / (float)windowHeight) * 2.0f;
    SDLVkInstance::GetInstance()->vertices[2].pos[0] = x;
    SDLVkInstance::GetInstance()->vertices[2].pos[1] = y;

    void* data;
    vkMapMemory(SDLVkInstance::GetInstance()->device,
                SDLVkInstance::GetInstance()->vertexBufferMemory, 0,
                sizeof(SDLVkInstance::GetInstance()->vertices), 0, &data);
    memcpy(data, SDLVkInstance::GetInstance()->vertices,
           sizeof(SDLVkInstance::GetInstance()->vertices));
    vkUnmapMemory(SDLVkInstance::GetInstance()->device,
                  SDLVkInstance::GetInstance()->vertexBufferMemory);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDLVkInstance::GetInstance()->window =
        SDL_CreateWindow("Dynamic Triangle", 800, 600, SDL_WINDOW_VULKAN);
    InitVulkan();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto sdlVkInstance = SDLVkInstance::GetInstance();
    vkDestroyBuffer(sdlVkInstance->device, sdlVkInstance->vertexBuffer,
                    nullptr);
    vkFreeMemory(sdlVkInstance->device, sdlVkInstance->vertexBufferMemory,
                 nullptr);
    vkDestroyDevice(sdlVkInstance->device, nullptr);
    vkDestroySurfaceKHR(sdlVkInstance->instance, sdlVkInstance->surface,
                        nullptr);
    vkDestroyInstance(sdlVkInstance->instance, nullptr);
    SDL_DestroyWindow(sdlVkInstance->window);
    SDL_Quit();
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        int width, height;
        SDL_GetWindowSize(SDLVkInstance::GetInstance()->window, &width,
                          &height);
        updateVertexBuffer(event->motion.x, event->motion.y, width, height);
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    return SDL_APP_CONTINUE;
}
