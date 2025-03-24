/*
 * @Author: Avidel_Arch 1215935448@qq.com
 * @Date: 2025-03-23 23:58:13
 * @LastEditors: nolanyzhang
 * @LastEditTime: 2025-03-25 00:26:58
 * @FilePath: /MiniRender/examples/03_triangle/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AEN
 */
#define SDL_MAIN_USE_CALLBACKS
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_main.h>

struct Vertex {
    float pos[2];
    float color[3];
};
class SDLVkInstance{
public:
    static SDLVkInstance* GetInstance(){
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
        {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 红色顶点
        {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // 绿色顶点
        {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}  // 蓝色顶点
    };

private: 
    SDLVkInstance() = default;
    ~SDLVkInstance() = default;  
};

void InitVulkan(){
    auto window = SDLVkInstance::GetInstance()->window;
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dynamic Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    std::cout << "Vulkan extension count: " << extensionCount << std::endl;
    for(uint32_t i = 0; i < extensionCount; ++i){
        const char* extensionName = SDL_Vulkan_GetInstanceExtensions(&extensionCount)[i];
        std::cout << "Vulkan extension: " << extensionName << std::endl;
    }
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]){
    SDL_Init(SDL_INIT_VIDEO);
    SDLVkInstance::GetInstance()->window = SDL_CreateWindow("Dynamic Triangle",  800, 600, SDL_WINDOW_VULKAN);
    InitVulkan();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result){
    SDL_Quit();
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    if(event->type == SDL_EVENT_QUIT){
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate){
    return SDL_APP_CONTINUE;
}
