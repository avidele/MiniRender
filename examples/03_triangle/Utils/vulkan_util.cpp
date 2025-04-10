/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#include "vulkan_util.hpp"
#include "SDL3/SDL_vulkan.h"
#include "spdlog/spdlog.h"

void VulkanContextManager::initVulkan(std::unique_ptr<SDLContext> sdl_context) {
    auto window = sdl_context->getWindow();
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dynamic Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t extension_count = 0;
    SDL_Vulkan_GetInstanceExtensions(&extension_count);
#if EnableDebug
    spdlog::info("Vulkan extension count: {}", extension_count);
    for (uint32_t i = 0; i < extension_count; ++i) {
        const char* extension_name =
            SDL_Vulkan_GetInstanceExtensions(&extension_count)[i];
        spdlog::info("Vulkan extension: {}", extension_name);
    }
#endif

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames =
            SDL_Vulkan_GetInstanceExtensions(&extension_count),
    };

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan instance!");
        exit(EXIT_FAILURE);
    }

    if (!SDL_Vulkan_CreateSurface(window.get(), instance, nullptr, &surface)) {
        spdlog::error("Failed to create Vulkan surface!");
        exit(EXIT_FAILURE);
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
#if EnableDebug
    spdlog::info("Vulkan device count: {}", device_count);
    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);
        spdlog::info("Vulkan device: {}", device_properties.deviceName);
    }
#endif
    physical_device = devices[0];

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
}