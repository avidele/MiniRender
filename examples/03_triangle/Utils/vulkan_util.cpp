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
    // 1. 获取SDL需要的扩展
    uint32_t extension_count = 0;

    const auto* m_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    std::vector<const char*> extensions(extension_count);
    for (uint32_t i = 0; i < extension_count; ++i) {
        extensions[i] = m_extensions[i];
    }
#if EnableDebug
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // 2. 获取验证层
    std::vector<const char*> validation_layers;
#if EnableDebug
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
    
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    
    bool layer_found = false;
    for (const auto& layer : available_layers) {
        if (strcmp("VK_LAYER_KHRONOS_validation", layer.layerName) == 0) {
            layer_found = true;
            break;
        }
    }
    
    if (!layer_found) {
        spdlog::warn("Validation layer VK_LAYER_KHRONOS_validation not found!");
        validation_layers.clear();  // 如果找不到，不使用验证层
    }
#endif

#if EnableDebug
    spdlog::info("Vulkan extension count: {}", extensions.size());
    spdlog::info("Vulkan extensions: ");
    for (const auto& extension : extensions) {
        spdlog::info("  {}", extension);
    }
    
    if (!validation_layers.empty()) {
        spdlog::info("Validation layers enabled:");
        for (const auto& layer : validation_layers) {
            spdlog::info("  {}", layer);
        }
    }
#endif

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

#if VKB_ENABLE_PORTABILITY
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

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
#if EnableDebug
    setupDebugMessenger();
#endif
    createSurface(std::move(sdl_context));
    initDevice();
}

#if EnableDebug
// 实现调试回调函数
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContextManager::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT  /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*  /*pUserData*/) {
    
    // 根据消息严重性级别使用不同的日志级别
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("Validation Error: {}", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("Validation Warning: {}", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        spdlog::info("Validation Info: {}", pCallbackData->pMessage);
    } else {
        spdlog::debug("Validation Debug: {}", pCallbackData->pMessage);
    }
    
    // 返回VK_FALSE表示不中断API调用
    return VK_FALSE;
}

// 设置调试信使
void VulkanContextManager::setupDebugMessenger() {
    // 创建调试信使的信息结构体
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    
    // 设置要接收的消息严重性级别
    create_info.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |  // 详细信息
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |     // 一般信息
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |  // 警告
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;     // 错误
    
    // 设置要接收的消息类型
    create_info.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |      // 一般消息
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |   // 验证消息
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;   // 性能消息
    
    // 设置回调函数和用户数据
    create_info.pfnUserCallback = debugCallback;
    create_info.pUserData = nullptr;
    
    // 获取函数指针(vkCreateDebugUtilsMessengerEXT不是核心功能，需要手动获取)
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT"));
    
    if (func) {
        if (func(instance, &create_info, nullptr, &debug_messenger) != VK_SUCCESS) {
            spdlog::error("Failed to set up debug messenger!");
        } else {
            spdlog::info("Debug messenger setup successfully");
        }
    } else {
        spdlog::error("Failed to find vkCreateDebugUtilsMessengerEXT function!");
    }
}
#endif

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

    std::vector<const char*> required_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if VKB_ENABLE_PORTABILITY
    required_device_extensions.push_back(
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // 正确的类型
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount =
            static_cast<uint32_t>(required_device_extensions.size()),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };

    if (VkResult result = vkCreateDevice(physical_device, &device_create_info,
                                         nullptr, &device);
        result != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan device: {}",
                      static_cast<int>(result));
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

void VulkanContextManager::createSurface(
    std::unique_ptr<SDLContext> sdl_context) {
    auto* window = sdl_context->getWindowPtr();
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        spdlog::error("Failed to create Vulkan surface! error: {} ",
                      SDL_GetError());
    }
#if EnableDebug
    spdlog::info("Vulkan surface created successfully.");
#endif
}

void VulkanContextManager::initSwapChain() {
    VkSwapchainCreateInfoKHR swap_chain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = 2,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {800, 600},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };

    if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                             &swap_chain) != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan swap chain!");
    }
}

void VulkanContextManager::clearVulkan() {
    #if EnableDebug
    // 销毁调试信使
    if (debug_messenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(instance, debug_messenger, nullptr);
        }
    }
#endif

    // 销毁其他Vulkan资源
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }
    
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
    
    spdlog::info("Vulkan resources cleared.");
}