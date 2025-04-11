/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#include "vulkan_util.hpp"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_vulkan.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

void VulkanContextManager::initVulkan(std::unique_ptr<SDLContext> sdl_context) {
    auto* window = sdl_context->getWindowPtr();
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dynamic Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    uint32_t extension_count = 0;
    // std::vector<const char*> extensions;
    const auto *extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
#if EnableDebug
    spdlog::info("Vulkan extension count: {}", extension_count);
    spdlog::info("Vulkan extensions: ");
    for (uint32_t i = 0; i < extension_count; ++i) {
        spdlog::info("Vulkan extension: {}", extensions[i]);
    }
#endif
    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
        result != VK_SUCCESS) {
        std::string error_msg;
        switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                error_msg = "Out of host memory";
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                error_msg = "Out of device memory";
                break;
            case VK_ERROR_INITIALIZATION_FAILED:
                error_msg = "Initialization failed";
                break;
            case VK_ERROR_LAYER_NOT_PRESENT:
                error_msg = "Layer not present";
                break;
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                error_msg = "Extension not present";
                break;
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                error_msg = "Incompatible driver";
                break;
            default:
                error_msg = "Unknown error";
                break;
        }
        spdlog::error("Failed to create Vulkan instance! Error code:  ({})",
                      error_msg);
        exit(EXIT_FAILURE);
    }
    createSurface(std::move(sdl_context));
    initDevice();
}

void VulkanContextManager::initDevice() {
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

    uint32_t device_extension_count;
    if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                             &device_extension_count,
                                             nullptr) != VK_SUCCESS) {
        spdlog::error("Failed to get Vulkan device extension count!");
        exit(EXIT_FAILURE);
    }

    std::vector<VkExtensionProperties> device_extensions(
        device_extension_count);
    if (vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, &device_extension_count,
            device_extensions.data()) != VK_SUCCESS) {
        spdlog::error("Failed to enumerate Vulkan device extensions!");
        exit(EXIT_FAILURE);
    }

    std::vector<const char *> required_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if VKB_ENABLE_PORTABILITY
    required_device_extensions.push_back(
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // 正确的类型
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };

    if (VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr,
                       &device); result != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan device: {}", static_cast<int>(result));
        exit(EXIT_FAILURE);
    }
    spdlog::info("Vulkan device created successfully.");
    uint32_t graphics_queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &graphics_queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> graphics_queue_properties(
        graphics_queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &graphics_queue_count,
                                             graphics_queue_properties.data());
#if EnableDebug
    spdlog::info("Vulkan graphics queue count: {}", graphics_queue_count);
#endif
    vkGetDeviceQueue(device, 0, 0, &graphics_queue);
}

void VulkanContextManager::createSurface(std::unique_ptr<SDLContext> sdl_context) {
    auto* window = sdl_context->getWindowPtr();
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        spdlog::error("Failed to create Vulkan surface! error: {} ", SDL_GetError());
    }
#if EnableDebug
    spdlog::info("Vulkan surface created successfully.");
#endif
}

void VulkanContextManager::clearVulkan() {
    vkDestroyInstance(instance, nullptr);
    spdlog::info("Vulkan resources cleared.");
}