/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#include "vulkan_util.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>  // For SDL_Event
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include <algorithm>  // For std::clamp
#include <chrono>     // For time
#include <cstdint>
#include <cstring>  // For strcmp
#include <fstream>  // For readFile
#include <set>      // For unique queue families
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <filesystem> // For absolute path in readFile
#include <fstream>    // For readFile
#include <stdexcept>  // For runtime_error
#define STB_IMAGE_IMPLEMENTATION // Define this in exactly one .c or .cpp file
#include <stb_image.h>           // For image loading

// --- SDLContext Implementation ---
bool SDLContext::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {  // Check return value correctly
        spdlog::error("Failed to initialize SDL: {}", SDL_GetError());
        return false;
    }
    window.reset(SDL_CreateWindow("Vulkan Triangle", width, height,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE));
    if (!window) {
        spdlog::error("Failed to create SDL window: {}", SDL_GetError());
        SDL_Quit();
        return false;
    }
    spdlog::info("SDL initialized and window created.");
    return true;
}

// --- VulkanContextManager Implementation ---

// Static instance initialization (if needed, depends on how getInstance is
// implemented) VulkanContextManager VulkanContextManager::instance; // If using
// a static member

void VulkanContextManager::initVulkan(SDL_Window* window) {
    if (!window) {
        throw std::runtime_error("SDL_Window pointer is null in initVulkan");
    }
    associated_window = window;  // Store the window pointer
    createInstance();
#if EnableDebug
    setupDebugMessenger();
#endif
    createSurface(window);  // Pass the window pointer
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();   // Creates swapchain, images, format, extent
    createImageViews();  // Creates image views based on swapchain images
}

void VulkanContextManager::cleanup() {
    cleanupSwapChain();  // Clean swapchain resources first

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
        spdlog::info("Logical device destroyed.");
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
        spdlog::info("Vulkan surface destroyed.");
    }

#if EnableDebug
    if (debug_messenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(instance, debug_messenger, nullptr);
            spdlog::info("Debug messenger destroyed.");
        }
        debug_messenger = VK_NULL_HANDLE;
    }
#endif

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        spdlog::info("Vulkan instance destroyed.");
    }
}

void VulkanContextManager::createInstance() {
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Triangle",  // Consistent name
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Redle Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,  // Example: Use 1.4
    };

    // Get required extensions from SDL
    uint32_t extension_count = 0;
    const auto* my_extensions =
        SDL_Vulkan_GetInstanceExtensions(&extension_count);
    std::vector<const char*> extensions(my_extensions,
                                        my_extensions + extension_count);

#if EnableDebug
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    spdlog::info("Debug utils extension enabled.");
#endif
#if VKB_ENABLE_PORTABILITY
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);  // Often
                                                                  // needed with
                                                                  // portability
    spdlog::info("Portability extensions enabled.");
#endif

    // Validation Layers
    std::vector<const char*> validation_layers;
#if EnableDebug
    const char* requested_layer = "VK_LAYER_KHRONOS_validation";
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    bool layer_found = false;
    for (const auto& layer : available_layers) {
        if (strcmp(requested_layer, layer.layerName) == 0) {
            layer_found = true;
            break;
        }
    }

    if (layer_found) {
        validation_layers.push_back(requested_layer);
        spdlog::info("Validation layer enabled: {}", requested_layer);
    } else {
        spdlog::warn("Validation layer {} not found!", requested_layer);
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

    // Debug messenger for instance creation/destruction
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
#if EnableDebug
    debug_create_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // Add VERBOSE and INFO if desired:
    // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = debugCallback;
    debug_create_info.pUserData =
        nullptr;  // Optional: pass 'this' or other data
    create_info.pNext =
        &debug_create_info;  // Chain the debug messenger create info
#endif

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
    spdlog::info("Vulkan instance created successfully.");

    // Log enabled extensions
    spdlog::info("Enabled instance extensions:");
    for (const char* ext_name : extensions) {
        spdlog::info("  - {}", ext_name);
    }
}

#if EnableDebug
// Implement debugCallback (static member)
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContextManager::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {
    // Simplified logging based on severity
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("Validation Layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >=
               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("Validation Layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >=
               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        // spdlog::info("Validation Layer: {}", pCallbackData->pMessage); //
        // Often too verbose
    } else {
        // spdlog::debug("Validation Layer: {}", pCallbackData->pMessage); //
        // Often too verbose
    }

    // Log message type (optional)
    // std::string type_str;
    // if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) type_str
    // += "GENERAL "; if (messageType &
    // VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) type_str += "VALIDATION
    // "; if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    // type_str += "PERFORMANCE "; spdlog::debug("Message Type: {}", type_str);

    return VK_FALSE;  // Don't abort the Vulkan call causing the message
}

void VulkanContextManager::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | // Enable for max
        // verbosity VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |    // Enable
        // for general info
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debugCallback;
    create_info.pUserData =
        nullptr;  // Or 'this' if callback needs instance data

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func) {
        if (func(instance, &create_info, nullptr, &debug_messenger) !=
            VK_SUCCESS) {
            spdlog::error("Failed to set up debug messenger!");
            // Don't throw, maybe just log the error
        } else {
            spdlog::info("Debug messenger set up successfully.");
        }
    } else {
        spdlog::error("vkCreateDebugUtilsMessengerEXT function not found!");
    }
}
#endif  // EnableDebug

void VulkanContextManager::createSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        throw std::runtime_error("Failed to create window surface: " +
                                 std::string(SDL_GetError()));
    }
    spdlog::info("Vulkan surface created successfully.");
}

void VulkanContextManager::pickPhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    spdlog::info("Available physical devices:");
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        const char* device_type = "Unknown";
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                device_type = "Other";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                device_type = "Integrated GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                device_type = "Discrete GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                device_type = "Virtual GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                device_type = "CPU";
                break;
            default:
                device_type = "Unknown";
                break;
        }

        spdlog::info("  - {} (Type: {})", properties.deviceName,
                     device_type);  // Log type too
        if (isDeviceSuitable(device)) {
            physical_device = device;
            // break; // Simple: pick the first suitable one
            // More complex: could score devices and pick the best (e.g., prefer
            // discrete GPU)
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    spdlog::info("Selected physical device: {}", properties.deviceName);
}

bool VulkanContextManager::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensions_supported = checkDeviceExtensionSupport(device);
    bool swapchain_adequate = false;
    if (extensions_supported) {
        SwapChainSupportDetails swapchain_support =
            querySwapChainSupport(device);
        swapchain_adequate = !swapchain_support.formats.empty() &&
                             !swapchain_support.present_modes.empty();
    }

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(
        device, &supported_features);  // Check for required features if any

    // 检查扩展动态状态特性是否可用
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
        extended_dynamic_state_features{};
    extended_dynamic_state_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &extended_dynamic_state_features;
    vkGetPhysicalDeviceFeatures2(device, &device_features2);

    return indices.isComplete() && extensions_supported && swapchain_adequate &&
           supported_features
               .samplerAnisotropy &&  // Example: require anisotropy
           extended_dynamic_state_features
               .extendedDynamicState;  // 确保支持扩展动态状态
}

bool VulkanContextManager::checkDeviceExtensionSupport(
    VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         available_extensions.data());

    std::vector<const char*> required_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME  // 添加扩展动态状态扩展
    };
#if VKB_ENABLE_PORTABILITY
    required_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    std::set<std::string> required(required_extensions.begin(),
                                   required_extensions.end());

    spdlog::debug("Available device extensions:");
    for (const auto& extension : available_extensions) {
        spdlog::debug("  - {}", extension.extensionName);
        required.erase(extension.extensionName);
    }

    if (!required.empty()) {
        spdlog::warn("Device missing required extensions:");
        for (const auto& req : required) {
            spdlog::warn("  - {}", req);
        }
    }

    return required.empty();
}

VulkanContextManager::QueueFamilyIndices
VulkanContextManager::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families.data());

    int i = 0;
    for (const auto& queue_family : queue_families) {
        // Check for graphics support
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }

        // Check for presentation support
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &present_support);
        if (present_support) {
            indices.present_family = i;
        }

        if (indices.isComplete()) {
            break;
        }
        i++;
    }
    return indices;
}

void VulkanContextManager::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physical_device);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {
        indices.graphics_family.value(), indices.present_family.value()};

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE; // Enable Anisotropy for sampler

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;

    // Enable required device extensions
    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if VKB_ENABLE_PORTABILITY
    device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    // Required for VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT
    device_extensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
#endif
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    // Modern Vulkan doesn't differentiate instance and device validation layers
    // if (enableValidationLayers) {
    //     create_info.enabledLayerCount =
    //     static_cast<uint32_t>(validationLayers.size());
    //     create_info.ppEnabledLayerNames = validationLayers.data();
    // } else {
    create_info.enabledLayerCount = 0;
    // }

    if (vkCreateDevice(physical_device, &create_info, nullptr, &device) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    spdlog::info("Logical device created.");

    // Get queue handles
    vkGetDeviceQueue(device, indices.graphics_family.value(), 0,
                     &graphics_queue);
    vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);
    spdlog::debug("Graphics and Present queue handles obtained.");
}

VulkanContextManager::SwapChainSupportDetails
VulkanContextManager::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    // Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);

    // Formats
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                         nullptr);
    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                             details.formats.data());
    }

    // Present Modes
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanContextManager::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        // Prefer SRGB for better color representation
        if (availableFormat.format ==
                VK_FORMAT_B8G8R8A8_SRGB &&  // Or R8G8B8A8_SRGB
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    // Fallback to the first available format
    return availableFormats[0];
}

VkPresentModeKHR VulkanContextManager::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        // Prefer Mailbox for low latency without tearing (if available)
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            spdlog::info("Using Present Mode: Mailbox");
            return availablePresentMode;
        }
    }
    // FIFO is guaranteed to be available (VSync)
    spdlog::info("Using Present Mode: FIFO");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContextManager::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        // If extent is fixed, use it
        return capabilities.currentExtent;
    } else {
        // Otherwise, get size from SDL and clamp to capabilities
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width,
                                  &height);  // Use this for high-DPI

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void VulkanContextManager::createSwapChain() {
    SwapChainSupportDetails swapchain_support =
        querySwapChainSupport(physical_device);

    VkSurfaceFormatKHR surface_format =
        chooseSwapSurfaceFormat(swapchain_support.formats);
    VkPresentModeKHR present_mode =
        chooseSwapPresentMode(swapchain_support.present_modes);
    VkExtent2D extent =
        chooseSwapExtent(swapchain_support.capabilities, associated_window);

    uint32_t image_count = swapchain_support.capabilities.minImageCount +
                           1;  // Request one more than minimum
    if (swapchain_support.capabilities.maxImageCount >
            0 &&  // maxImageCount == 0 means no limit
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;  // Non-stereoscopic
    create_info.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // Basic usage

    // Add transfer usage if supported and needed (e.g., for screenshots)
    if (swapchain_support.capabilities.supportedUsageFlags &
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (swapchain_support.capabilities.supportedUsageFlags &
        VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    QueueFamilyIndices indices = findQueueFamilies(physical_device);
    uint32_t queue_family_indices[] = {indices.graphics_family.value(),
                                       indices.present_family.value()};

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode =
            VK_SHARING_MODE_CONCURRENT;  // Easier for separate queues
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
        spdlog::info("Swapchain using Concurrent sharing mode.");
    } else {
        create_info.imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE;  // Better performance if queues are same
        create_info.queueFamilyIndexCount = 0;      // Optional
        create_info.pQueueFamilyIndices = nullptr;  // Optional
        spdlog::info("Swapchain using Exclusive sharing mode.");
    }

    create_info.preTransform = swapchain_support.capabilities
                                   .currentTransform;  // Use current transform
    create_info.compositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // No blending with window system
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;  // Allow clipping obscured pixels
    create_info.oldSwapchain =
        VK_NULL_HANDLE;  // No old swapchain on initial creation

    if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }
    spdlog::info("Swapchain created successfully.");

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count,
                            nullptr);  // Get actual count
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count,
                            swapchain_images.data());
    spdlog::info("Retrieved {} swapchain images.", image_count);

    // Store format and extent
    swapchain_image_format = surface_format.format;
    swapchain_extent = extent;
}

void VulkanContextManager::createImageViews() {
    swapchain_image_views.resize(swapchain_images.size());

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        // Use the new helper function
        swapchain_image_views[i] = createImageView(swapchain_images[i], swapchain_image_format);
    }
    spdlog::info("Created {} swapchain image views.",
                 swapchain_image_views.size());
}

void VulkanContextManager::cleanupSwapChain() {
    spdlog::debug("Cleaning up swapchain...");
    // Image views first
    for (auto image_view : swapchain_image_views) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, image_view, nullptr);
        }
    }
    swapchain_image_views.clear();
    spdlog::debug("Swapchain image views destroyed.");

    // Then swapchain itself
    if (swap_chain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swap_chain, nullptr);
        swap_chain = VK_NULL_HANDLE;
        spdlog::debug("Swapchain destroyed.");
    }
    // swapchain_images are owned by the swapchain, no need to destroy them
    // explicitly
    swapchain_images.clear();
}

void VulkanContextManager::recreateSwapChain() {
    spdlog::info("Recreating swapchain...");
    // Handle minimization (wait until window is restored)
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(associated_window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSizeInPixels(associated_window, &width, &height);
        SDL_WaitEvent(nullptr);  // Wait for events like resize/restore
    }

    vkDeviceWaitIdle(
        device);  // Wait until device is idle before destroying old resources

    cleanupSwapChain();  // Destroy old swapchain and image views

    // Recreate swapchain and image views with new size/properties
    createSwapChain();
    createImageViews();
    spdlog::info("Swapchain recreated successfully.");
    // Note: Framebuffers and potentially command buffers need to be recreated
    // by the Renderer
}

// --- Utility Function Implementations ---

uint32_t VulkanContextManager::findMemoryType(
    uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Check if the bit for this memory type is set in the filter
        // AND check if the required property flags are met by this memory type
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;  // Found a suitable memory type
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanContextManager::createBuffer(VkDeviceSize size,
                                        VkBufferUsageFlags usage,
                                        VkMemoryPropertyFlags properties,
                                        VkBuffer& buffer,
                                        VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode =
        VK_SHARING_MODE_EXCLUSIVE;  // Or CONCURRENT if needed

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    // Bind the memory to the buffer object
    // The last parameter (offset) is 0 because we allocate memory specifically
    // for this buffer
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer VulkanContextManager::beginSingleTimeCommands(
    VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;  // Use the provided pool
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Important flag

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanContextManager::endSingleTimeCommands(
    VkCommandPool pool, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit to the graphics queue (assuming it supports transfer operations)
    vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);  // Wait for the transfer to complete

    vkFreeCommandBuffers(device, pool, 1,
                         &commandBuffer);  // Clean up the command buffer
}

void VulkanContextManager::copyBuffer(VkCommandPool pool, VkBuffer srcBuffer,
                                      VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(pool);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;  // Optional
    copyRegion.dstOffset = 0;  // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(pool, commandBuffer);
}

VkImageView VulkanContextManager::createImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(device, &view_info, nullptr, &image_view) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
    return image_view;
}

void VulkanContextManager::createImage(uint32_t width, uint32_t height, VkFormat format,
                                     VkImageTiling tiling, VkImageUsageFlags usage,
                                     VkMemoryPropertyFlags properties, VkImage& image,
                                     VkDeviceMemory& imageMemory) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc_info, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void VulkanContextManager::transitionImageLayout(VkCommandPool pool, VkImage image, VkFormat format,
                                               VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer command_buffer = beginSingleTimeCommands(pool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage, destination_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(pool, command_buffer);
}

void VulkanContextManager::copyBufferToImage(VkCommandPool pool, VkBuffer buffer, VkImage image,
                                           uint32_t width, uint32_t height) {
    VkCommandBuffer command_buffer = beginSingleTimeCommands(pool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Image must be in this layout
        1,
        &region
    );

    endSingleTimeCommands(pool, command_buffer);
}

// --- Renderer Implementation ---

Renderer::Renderer(VulkanContextManager* context) : vulkan_context(context) {
    if (!vulkan_context) {
        throw std::invalid_argument(
            "VulkanContextManager pointer cannot be null for Renderer");
    }
}

Renderer::~Renderer() {
    // RAII cleanup if not called explicitly
     try {
         if (vulkan_context && vulkan_context->getDevice() != VK_NULL_HANDLE) {
             vkDeviceWaitIdle(vulkan_context->getDevice());
         }
         cleanup();
     } catch (const std::exception& e) {
         spdlog::error("Error during Renderer destructor cleanup: {}", e.what());
     } catch (...) {
         spdlog::error("Unknown error during Renderer destructor cleanup.");
     }
}

void Renderer::init() {
    spdlog::info("Initializing Renderer...");
    createCommandPool();          // Pool needed early for buffer/image copies
    createTextureImage();         // Load and upload texture
    createTextureImageView();     // Create view for the texture
    createTextureSampler();       // Create sampler for the texture
    createVertexBuffer();         // Create vertex buffer
    createIndexBuffer();          // Create index buffer
    createDescriptorSetLayout();  // Must be before pipeline layout
    createRenderPass();
    createGraphicsPipeline();     // Depends on layout and render pass
    createFramebuffers();         // Depends on swapchain image views and render pass
    createUniformBuffers();       // Create UBOs
    createDescriptorPool();       // Create pool for descriptor sets
    createDescriptorSets();       // Allocate and bind descriptor sets (UBO + Texture)
    createCommandBuffers();       // Depends on framebuffers, pipeline, etc.
    createSyncObjects();
    spdlog::info("Renderer initialized successfully.");
}

void Renderer::cleanup() {
    spdlog::info("Cleaning up Renderer...");
    // Wait for device idle before destroying resources
    // vkDeviceWaitIdle called in destructor or before explicit call

    cleanupSwapChainDependents();  // Clean things that depend on the swapchain first

    // Destroy Texture Sampler
    if (texture_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vulkan_context->getDevice(), texture_sampler, nullptr);
        texture_sampler = VK_NULL_HANDLE;
        spdlog::debug("Texture sampler destroyed.");
    }
    // Destroy Texture Image View
    if (texture_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(vulkan_context->getDevice(), texture_image_view, nullptr);
        texture_image_view = VK_NULL_HANDLE;
        spdlog::debug("Texture image view destroyed.");
    }
    // Destroy Texture Image
    if (texture_image != VK_NULL_HANDLE) {
        vkDestroyImage(vulkan_context->getDevice(), texture_image, nullptr);
        texture_image = VK_NULL_HANDLE;
    }
    // Free Texture Image Memory
    if (texture_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vulkan_context->getDevice(), texture_image_memory, nullptr);
        texture_image_memory = VK_NULL_HANDLE;
    }
     spdlog::debug("Texture image and memory destroyed.");


    // Destroy UBOs and their memory
    for (size_t i = 0; i < uniform_buffers.size(); i++) {
        if (uniform_buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkan_context->getDevice(), uniform_buffers[i],
                            nullptr);
        }
        if (uniform_buffers_memory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(vulkan_context->getDevice(), uniform_buffers_memory[i],
                         nullptr);
        }
    }
    uniform_buffers.clear();
    uniform_buffers_memory.clear();
    spdlog::debug("Uniform buffers destroyed.");

    // Destroy descriptor set layout (after pipelines)
    if (descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vulkan_context->getDevice(),
                                     descriptor_set_layout, nullptr);
        descriptor_set_layout = VK_NULL_HANDLE;
        spdlog::debug("Descriptor set layout destroyed.");
    }

    // Destroy index buffer
    if (index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vulkan_context->getDevice(), index_buffer, nullptr);
        index_buffer = VK_NULL_HANDLE;
    }
    if (index_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vulkan_context->getDevice(), index_buffer_memory, nullptr);
        index_buffer_memory = VK_NULL_HANDLE;
    }
    spdlog::debug("Index buffer destroyed.");


    // Destroy vertex buffer
    if (vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vulkan_context->getDevice(), vertex_buffer, nullptr);
        vertex_buffer = VK_NULL_HANDLE;
    }
    if (vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vulkan_context->getDevice(), vertex_buffer_memory,
                     nullptr);
        vertex_buffer_memory = VK_NULL_HANDLE;
    }
    spdlog::debug("Vertex buffer destroyed.");

    // Destroy synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (render_finished_semaphores.size() > i && render_finished_semaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(vulkan_context->getDevice(),
                               render_finished_semaphores[i], nullptr);
        if (image_available_semaphores.size() > i && image_available_semaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(vulkan_context->getDevice(),
                               image_available_semaphores[i], nullptr);
        if (in_flight_fences.size() > i && in_flight_fences[i] != VK_NULL_HANDLE)
            vkDestroyFence(vulkan_context->getDevice(), in_flight_fences[i],
                           nullptr);
    }
    render_finished_semaphores.clear();
    image_available_semaphores.clear();
    in_flight_fences.clear();
    spdlog::debug("Synchronization objects destroyed.");

    // Destroy command pool (destroys command buffers allocated from it)
    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vulkan_context->getDevice(), command_pool,
                             nullptr);
        command_pool = VK_NULL_HANDLE;
        spdlog::debug("Command pool destroyed.");
    }
    // No need to explicitly destroy command buffers if pool is destroyed

    // Note: Render pass, pipeline layout, pipeline, descriptor pool are cleaned
    // in cleanupSwapChainDependents or here if not dependent

    spdlog::info("Renderer cleanup complete.");
}

void Renderer::cleanupSwapChainDependents() {
    spdlog::debug("Cleaning up swapchain-dependent resources...");
    // Framebuffers
    for (auto framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vulkan_context->getDevice(), framebuffer,
                                 nullptr);
        }
    }
    swapchain_framebuffers.clear();
    spdlog::debug("Framebuffers destroyed.");

    // Descriptor Pool (needs recreation because count depends on swapchain images for UBOs)
    if (descriptor_pool != VK_NULL_HANDLE) {
        // Don't vkResetDescriptorPool, just destroy it. Sets are implicitly freed.
        vkDestroyDescriptorPool(vulkan_context->getDevice(), descriptor_pool,
                                nullptr);
        descriptor_pool = VK_NULL_HANDLE;
        spdlog::debug("Descriptor pool destroyed.");
    }
    descriptor_sets.clear(); // The vector itself needs clearing

    // Command Buffers (Freed with command pool destruction, just clear vector)
    // If command buffers were allocated with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    // and reset individually, they might need explicit freeing here.
    // But since we reset the pool, they are gone.
    command_buffers.clear(); // Clear the vector


    // Graphics Pipeline
    if (graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vulkan_context->getDevice(), graphics_pipeline,
                          nullptr);
        graphics_pipeline = VK_NULL_HANDLE;
        spdlog::debug("Graphics pipeline destroyed.");
    }

    // Pipeline Layout (Depends on descriptor set layout, which might change indirectly)
    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vulkan_context->getDevice(), pipeline_layout,
                                nullptr);
        pipeline_layout = VK_NULL_HANDLE;
        spdlog::debug("Pipeline layout destroyed.");
    }

    // Render Pass (Might depend on swapchain format)
    if (render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vulkan_context->getDevice(), render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
        spdlog::debug("Render pass destroyed.");
    }
    // Image views and swapchain itself are handled by
    // VulkanContextManager::cleanupSwapChain
}

void Renderer::handleSwapChainRecreation() {
    cleanupSwapChainDependents();  // Clean old resources first

    // Recreate resources that depend on the new swapchain properties
    createRenderPass();           // Might depend on new format
    // DescriptorSetLayout usually doesn't depend on swapchain, but pipeline does
    // createDescriptorSetLayout(); // Recreate layout only if it changes
    createGraphicsPipeline();     // Depends on layout and render pass
    createFramebuffers();         // Depends on new image views and render pass
    createUniformBuffers();       // Recreate UBOs for the new number of images
    createDescriptorPool();       // Recreate pool for new number of sets
    createDescriptorSets();       // Recreate descriptor sets (bind UBOs and Texture)
    createCommandBuffers();       // Depends on framebuffers, pipeline, etc.
}

void Renderer::createRenderPass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = vulkan_context->getSwapChainImageFormat();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;  // No multisampling yet
    color_attachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear framebuffer before drawing
    color_attachment.storeOp =
        VK_ATTACHMENT_STORE_OP_STORE;  // Store result to be presented
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED;  // Layout before render pass
    color_attachment.finalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Layout after render pass (for
                                          // presentation)

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment =
        0;  // Index of the attachment in the pAttachments array
    color_attachment_ref.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // Layout during the subpass

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    // pDepthStencilAttachment = nullptr; // No depth buffer yet

    // Subpass dependency to handle layout transitions
    VkSubpassDependency dependency{};
    dependency.srcSubpass =
        VK_SUBPASS_EXTERNAL;    // Implicit subpass before render pass
    dependency.dstSubpass = 0;  // Our first (and only) subpass
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // Wait for color output
                                                        // stage
    dependency.srcAccessMask = 0;  // No access needed before
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // Stage where writes
                                                        // happen
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // We will write to the color
                                               // attachment

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(vulkan_context->getDevice(), &render_pass_info,
                           nullptr, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    spdlog::debug("Render pass created.");
}

// 修改：创建描述符集布局 (添加纹理采样器绑定)
void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 0;  // layout(binding = 0) in shader (UBO)
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // UBO used in both
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding = 1; // layout(binding = 1) in shader (Sampler)
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.pImmutableSamplers = nullptr;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Sampler used in fragment shader

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {ubo_layout_binding, sampler_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkan_context->getDevice(), &layout_info,
                                    nullptr,
                                    &descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    spdlog::debug("Descriptor set layout created with UBO and Sampler bindings.");
}

std::vector<char> Renderer::readFile(const std::string& filename) {
    // Open file at the end to easily get the size
    std::filesystem::path absolute_path = std::filesystem::absolute(filename);
    spdlog::debug("Attempting to read file from absolute path: {}",
                  absolute_path.string());
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);  // Go back to the beginning
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    // Note: pointer needs to be uint32_t*, safe cast if vector stores char/byte
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module;
    if (vkCreateShaderModule(vulkan_context->getDevice(), &create_info, nullptr,
                             &shader_module) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shader_module;
}

void Renderer::createGraphicsPipeline() {
    // 使用相对路径或确保工作目录正确
    std::string shader_dir = "./shaders/";  // 使用相对路径
    auto vert_shader_code = readFile(shader_dir + "vert.spv");
    auto frag_shader_code = readFile(shader_dir + "frag.spv");

    VkShaderModule vert_shader_module = createShaderModule(vert_shader_code);
    VkShaderModule frag_shader_module = createShaderModule(frag_shader_code);
    spdlog::debug("Shader modules created.");

    // --- Shader Stage Creation ---
    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";  // Entry point function name

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                       frag_shader_stage_info};

    // --- Vertex Input State ---
    auto binding_description = Vertex::getBindingDescription();
    auto attribute_descriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions =
        attribute_descriptions.data();

    // --- Input Assembly State ---
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // 默认设置为三角形列表，稍后会动态更改
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // --- Viewport and Scissor --- (Dynamic state, but need placeholder here)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;  // One viewport
    viewport_state.scissorCount = 1;   // One scissor rectangle

    // --- Rasterization State ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // 用于三角形
    rasterizer.lineWidth = 1.0f;  // 点的大小通过 gl_PointSize 控制
    rasterizer.cullMode = VK_CULL_MODE_NONE; // Or VK_CULL_MODE_BACK_BIT if you want back-face culling
    rasterizer.frontFace =
        VK_FRONT_FACE_CLOCKWISE;  // Changed from COUNTER_CLOCKWISE to CLOCKWISE
    rasterizer.depthBiasEnable = VK_FALSE;

    // --- Multisampling State ---
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- Depth/Stencil State --- (Not used in this example)
    // VkPipelineDepthStencilStateCreateInfo depthStencil{};

    // --- Color Blend State ---
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // --- Dynamic State ---
    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT  // 添加动态拓扑状态
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount =
        static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // --- Pipeline Layout ---
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;  // 使用一个描述符集布局
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;  // 关联布局
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(vulkan_context->getDevice(),
                               &pipeline_layout_info, nullptr,
                               &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
    spdlog::debug("Pipeline layout created.");

    // --- Graphics Pipeline Creation ---
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;

    if (vkCreateGraphicsPipelines(vulkan_context->getDevice(), VK_NULL_HANDLE,
                                  1, &pipeline_info, nullptr,
                                  &graphics_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    spdlog::debug("Graphics pipeline created.");

    // --- Cleanup Shader Modules ---
    vkDestroyShaderModule(vulkan_context->getDevice(), frag_shader_module,
                          nullptr);
    vkDestroyShaderModule(vulkan_context->getDevice(), vert_shader_module,
                          nullptr);
    spdlog::debug("Shader modules destroyed.");
}

void Renderer::createFramebuffers() {
    const auto& swapchain_views = vulkan_context->getSwapChainImageViews();
    swapchain_framebuffers.resize(swapchain_views.size());

    for (size_t i = 0; i < swapchain_views.size(); i++) {
        VkImageView attachments[] = {swapchain_views[i]};

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;  // Compatible render pass
        framebuffer_info.attachmentCount = 1;       // One attachment (color)
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vulkan_context->getSwapChainExtent().width;
        framebuffer_info.height = vulkan_context->getSwapChainExtent().height;
        framebuffer_info.layers = 1;  // Number of layers in image arrays

        if (vkCreateFramebuffer(vulkan_context->getDevice(), &framebuffer_info,
                                nullptr,
                                &swapchain_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
    spdlog::debug("Created {} framebuffers.", swapchain_framebuffers.size());
}

void Renderer::createCommandPool() {
    VulkanContextManager::QueueFamilyIndices queue_family_indices =
        vulkan_context->findQueueFamilies(vulkan_context->getPhysicalDevice());

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // Allow resetting
                                                          // individual command
                                                          // buffers
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family
                                     .value();  // Pool for graphics commands

    if (vkCreateCommandPool(vulkan_context->getDevice(), &pool_info, nullptr,
                            &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
    spdlog::debug("Command pool created.");
}

void Renderer::createVertexBuffer() {
    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    // Create a staging buffer (CPU visible)
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vulkan_context->createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging_buffer, staging_buffer_memory);

    // Map memory, copy data, unmap
    void* data;
    vkMapMemory(vulkan_context->getDevice(), staging_buffer_memory, 0,
                buffer_size, 0, &data);
    memcpy(data, vertices.data(), (size_t)buffer_size);
    vkUnmapMemory(vulkan_context->getDevice(), staging_buffer_memory);

    // Create the actual vertex buffer (GPU local)
    vulkan_context->createBuffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer,
        vertex_buffer_memory);

    // Copy data from staging buffer to vertex buffer
    vulkan_context->copyBuffer(command_pool, staging_buffer, vertex_buffer,
                               buffer_size);

    // Clean up staging buffer
    vkDestroyBuffer(vulkan_context->getDevice(), staging_buffer, nullptr);
    vkFreeMemory(vulkan_context->getDevice(), staging_buffer_memory, nullptr);
    spdlog::debug("Vertex buffer created and data transferred.");
}

void Renderer::createTextureImage() {
    int tex_width, tex_height, tex_channels;
    // Use relative path from executable or ensure correct working directory
    std::string image_path_str = "Image/texture.png";
    std::filesystem::path image_path = std::filesystem::absolute(image_path_str);
    spdlog::info("Attempting to load texture from: {}", image_path.string());

    // Load image using stb_image
    stbi_uc* pixels = stbi_load(image_path.string().c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    VkDeviceSize image_size = tex_width * tex_height * 4; // 4 bytes per pixel (RGBA)

    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + image_path.string());
    }
     spdlog::info("Texture loaded: {}x{} ({} channels)", tex_width, tex_height, tex_channels);


    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vulkan_context->createBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging_buffer, staging_buffer_memory);

    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(vulkan_context->getDevice(), staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(image_size));
    vkUnmapMemory(vulkan_context->getDevice(), staging_buffer_memory);

    // Free CPU-side pixel data
    stbi_image_free(pixels);

    // Create the Vulkan image object
    vulkan_context->createImage(tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB, // Use SRGB format for color data
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Need transfer dst and sampled usage
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                texture_image, texture_image_memory);

    // Transition image layout for transfer
    vulkan_context->transitionImageLayout(command_pool, texture_image, VK_FORMAT_R8G8B8A8_SRGB,
                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy data from staging buffer to image
    vulkan_context->copyBufferToImage(command_pool, staging_buffer, texture_image,
                                    static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));

    // Transition image layout for shader access
    vulkan_context->transitionImageLayout(command_pool, texture_image, VK_FORMAT_R8G8B8A8_SRGB,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Clean up staging buffer
    vkDestroyBuffer(vulkan_context->getDevice(), staging_buffer, nullptr);
    vkFreeMemory(vulkan_context->getDevice(), staging_buffer_memory, nullptr);

    spdlog::debug("Texture image created and data transferred.");
}

void Renderer::createTextureImageView() {
    texture_image_view = vulkan_context->createImageView(texture_image, VK_FORMAT_R8G8B8A8_SRGB);
    spdlog::debug("Texture image view created.");
}

void Renderer::createTextureSampler() {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR; // How to interpolate texels that are magnified
    sampler_info.minFilter = VK_FILTER_LINEAR; // How to interpolate texels that are minified

    // Addressing mode
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // Anisotropy
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vulkan_context->getPhysicalDevice(), &properties);
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // Use max supported level

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // Border color for clamp mode
    sampler_info.unnormalizedCoordinates = VK_FALSE; // Use normalized coordinates [0, 1)

    // Compare function (used for percentage-closer filtering for shadow maps)
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

    // Mipmapping (not used here)
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    if (vkCreateSampler(vulkan_context->getDevice(), &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    spdlog::debug("Texture sampler created.");
}

void Renderer::createIndexBuffer() {
    VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vulkan_context->createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging_buffer, staging_buffer_memory);

    // Copy index data to staging buffer
    void* data;
    vkMapMemory(vulkan_context->getDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, indices.data(), (size_t)buffer_size);
    vkUnmapMemory(vulkan_context->getDevice(), staging_buffer_memory);

    // Create the actual index buffer (GPU local)
    vulkan_context->createBuffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, // Add index buffer usage
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        index_buffer, index_buffer_memory);

    // Copy data from staging buffer to index buffer
    vulkan_context->copyBuffer(command_pool, staging_buffer, index_buffer, buffer_size);

    // Clean up staging buffer
    vkDestroyBuffer(vulkan_context->getDevice(), staging_buffer, nullptr);
    vkFreeMemory(vulkan_context->getDevice(), staging_buffer_memory, nullptr);
    spdlog::debug("Index buffer created and data transferred.");
}

// 新增：创建 Uniform Buffers
void Renderer::createUniformBuffers() {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    size_t swapchain_image_count = vulkan_context->getSwapChainImages().size();
    uniform_buffers.resize(swapchain_image_count);
    uniform_buffers_memory.resize(swapchain_image_count);

    for (size_t i = 0; i < swapchain_image_count; i++) {
        vulkan_context->createBuffer(
            buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniform_buffers[i], uniform_buffers_memory[i]);
    }
    spdlog::debug("Created {} uniform buffers.", uniform_buffers.size());
}

// 修改：创建描述符池 (增加采样器类型)
void Renderer::createDescriptorPool() {
    size_t swapchain_image_count = vulkan_context->getSwapChainImages().size();

    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    // UBO Pool Size
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = static_cast<uint32_t>(swapchain_image_count);
    // Sampler Pool Size
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = static_cast<uint32_t>(swapchain_image_count); // One sampler per frame/set

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(swapchain_image_count); // Max number of sets we can allocate

    if (vkCreateDescriptorPool(vulkan_context->getDevice(), &pool_info, nullptr,
                               &descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    spdlog::debug("Descriptor pool created for UBOs and Samplers.");
}

// 修改：创建描述符集 (绑定 UBO 和纹理采样器)
void Renderer::createDescriptorSets() {
    size_t swapchain_image_count = vulkan_context->getSwapChainImages().size();
    std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count,
                                               descriptor_set_layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount =
        static_cast<uint32_t>(swapchain_image_count);
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets.resize(swapchain_image_count);
    if (vkAllocateDescriptorSets(vulkan_context->getDevice(), &alloc_info,
                                 descriptor_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    spdlog::debug("Allocated {} descriptor sets.", descriptor_sets.size());

    // Update each descriptor set
    for (size_t i = 0; i < swapchain_image_count; i++) {
        // --- UBO Binding ---
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        // --- Sampler Binding ---
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = texture_image_view;
        image_info.sampler = texture_sampler;

        std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

        // Write for UBO
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0; // binding = 0 (UBO)
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        // Write for Sampler
        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = descriptor_sets[i];
        descriptor_writes[1].dstBinding = 1; // binding = 1 (Sampler)
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(vulkan_context->getDevice(),
                               static_cast<uint32_t>(descriptor_writes.size()),
                               descriptor_writes.data(), 0, nullptr);
    }
    spdlog::debug("Updated descriptor sets with UBO and Sampler info.");
}

void Renderer::createCommandBuffers() {
    command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // Main command buffers
    alloc_info.commandBufferCount = (uint32_t)command_buffers.size();

    if (vkAllocateCommandBuffers(vulkan_context->getDevice(), &alloc_info,
                                 command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    spdlog::debug("Allocated {} command buffers.", command_buffers.size());
    // Recording happens per-frame in drawFrame
}

// 新增：更新 Uniform Buffer
void Renderer::updateUniformBuffer(uint32_t currentImage) {
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     current_time - start_time)
                     .count();

    UniformBufferObject ubo{};

    // 模型矩阵：保持不变（单位矩阵）或根据需要旋转/缩放模型
    ubo.model = glm::mat4(1.0f);
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
    // glm::vec3(0.0f, 0.0f, 1.0f));

    // 视图矩阵：相机围绕 Y 轴旋转
    float radius = 2.0f;
    float camX = sin(time * glm::radians(45.0f)) * radius;
    float camZ = cos(time * glm::radians(45.0f)) * radius;
    ubo.view = glm::lookAt(glm::vec3(camX, 0.0f, camZ),   // Eye position
                           glm::vec3(0.0f, 0.0f, 0.0f),   // Center position
                           glm::vec3(0.0f, 1.0f, 0.0f));  // Up direction

    // 投影矩阵：透视投影
    VkExtent2D extent = vulkan_context->getSwapChainExtent();
    ubo.proj = glm::perspective(
        glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
    // GLM 设计用于 OpenGL，其 Y 坐标是反的。Vulkan 中需要翻转 Y 轴。
    ubo.proj[1][1] *= -1;

    // 动态颜色：随时间在红绿蓝之间循环
    ubo.lightColor.r = (sin(time * 1.0f) + 1.0f) / 2.0f;
    ubo.lightColor.g = (sin(time * 0.7f + glm::radians(120.0f)) + 1.0f) / 2.0f;
    ubo.lightColor.b = (sin(time * 0.4f + glm::radians(240.0f)) + 1.0f) / 2.0f;
    // 保证亮度
    // ubo.lightColor = glm::normalize(ubo.lightColor) * 0.5f + 0.5f;

    // 复制数据到 Uniform Buffer
    void* data;
    vkMapMemory(vulkan_context->getDevice(),
                uniform_buffers_memory[currentImage], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vulkan_context->getDevice(),
                  uniform_buffers_memory[currentImage]);
}

void Renderer::recordCommandBuffer(VkCommandBuffer command_buffer,
                                   uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Start Render Pass
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = swapchain_framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = vulkan_context->getSwapChainExtent();

    VkClearValue clear_color = {{{0.1f, 0.1f, 0.1f, 1.0f}}}; // Dark grey clear color
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    // Bind Graphics Pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphics_pipeline);

    // Bind Vertex Buffer
    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    // Bind Index Buffer
    vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16); // Use uint16 indices

    // Bind Descriptor Set (UBO + Sampler)
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1,
                            &descriptor_sets[image_index], 0, nullptr);

    // Set Dynamic Viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width =
        static_cast<float>(vulkan_context->getSwapChainExtent().width);
    viewport.height =
        static_cast<float>(vulkan_context->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    // Set Dynamic Scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vulkan_context->getSwapChainExtent();
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Set Dynamic Primitive Topology (Fix for validation error)
    // Since VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT is enabled, we must set it.
    // Get the function pointer for vkCmdSetPrimitiveTopologyEXT
    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT) vkGetDeviceProcAddr(vulkan_context->getDevice(), "vkCmdSetPrimitiveTopologyEXT");
    if (vkCmdSetPrimitiveTopologyEXT) {
        vkCmdSetPrimitiveTopologyEXT(command_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    } else {
        // Fallback or error handling if function pointer is not found
        // If using Vulkan 1.3+, vkCmdSetPrimitiveTopology might be available directly
        // Check your Vulkan header version and loader.
        spdlog::warn("vkCmdSetPrimitiveTopologyEXT function pointer not found! Trying vkCmdSetPrimitiveTopology...");
        // Assuming Vulkan 1.3+ core function is available as a fallback
        // Note: This requires Vulkan headers version 1.3 or higher.
        // If you are sure you are on 1.3+, you could use this directly.
        // vkCmdSetPrimitiveTopology(command_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        // For now, just log a warning if EXT is not found.
    }

    // Draw Indexed Square
    vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    // End Render Pass
    vkCmdEndRenderPass(command_buffer);

    // End Recording
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Renderer::createSyncObjects() {
    image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    // images_in_flight is sized based on swapchain image count, done in
    // drawFrame initialization

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled so first
                                                      // frame doesn't wait

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(vulkan_context->getDevice(), &semaphore_info,
                              nullptr,
                              &image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vulkan_context->getDevice(), &semaphore_info,
                              nullptr,
                              &render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkan_context->getDevice(), &fence_info, nullptr,
                          &in_flight_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create synchronization objects for a frame!");
        }
    }
    spdlog::debug("Created {} sets of synchronization objects.",
                  MAX_FRAMES_IN_FLIGHT);
}

void Renderer::drawFrame() {
    VkDevice device = vulkan_context->getDevice();
    VkSwapchainKHR swapchain = vulkan_context->getSwapChain();
    VkQueue graphics_queue = vulkan_context->getGraphicsQueue();
    VkQueue present_queue = vulkan_context->getPresentQueue();

    // 1. Wait for the previous frame to finish (CPU-GPU sync)
    vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE,
                    UINT64_MAX);

    // 2. Acquire an image from the swap chain
    uint32_t image_index;
    VkResult result =
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                              image_available_semaphores[current_frame],
                              VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        spdlog::warn("Swapchain out of date during acquire, recreating...");
        vulkan_context->recreateSwapChain();
        handleSwapChainRecreation();
        framebuffer_resized = false;
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // --- Update Uniform Buffer ---
    updateUniformBuffer(image_index);  // 更新当前图像索引对应的 UBO

    // Check if a previous frame is still using this image
    if (images_in_flight.size() <= image_index) {
        images_in_flight.resize(image_index + 1, VK_NULL_HANDLE);
    }
    if (images_in_flight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE,
                        UINT64_MAX);
    }
    images_in_flight[image_index] = in_flight_fences[current_frame];

    // 3. Record the command buffer
    vkResetCommandBuffer(command_buffers[current_frame], 0);
    recordCommandBuffer(command_buffers[current_frame], image_index);

    // 4. Submit the command buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[current_frame];

    VkSemaphore signal_semaphores[] = {
        render_finished_semaphores[current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device, 1, &in_flight_fences[current_frame]);

    if (vkQueueSubmit(graphics_queue, 1, &submit_info,
                      in_flight_fences[current_frame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // 5. Present the image
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        framebuffer_resized) {
        spdlog::warn("Swapchain out of date or suboptimal during present, or "
                     "window resized. Recreating...");
        framebuffer_resized = false;
        vulkan_context->recreateSwapChain();
        handleSwapChainRecreation();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    // Advance to the next frame index
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// --- TriangleApplication Implementation ---

void TriangleApplication::run() {
    try {
        initWindow();
        initVulkan();
        mainLoop();
    } catch (const std::exception& e) {
        spdlog::critical("Application error: {}", e.what());
        // Cleanup might be necessary even after an exception
    }
    cleanup();  // Ensure cleanup happens
}

void TriangleApplication::initWindow() {
    sdl_context = std::make_unique<SDLContext>(800, 600);  // Or desired size
    if (!sdl_context->init()) {
        throw std::runtime_error("Failed to initialize SDL context");
    }
}

void TriangleApplication::initVulkan() {
    vulkan_manager = VulkanContextManager::getInstance();
    vulkan_manager->initVulkan(sdl_context->getWindowPtr());

    renderer = std::make_unique<Renderer>(vulkan_manager);
    renderer->init();
}

void TriangleApplication::mainLoop() {
    SDL_Event e;
    app_running = true;
    while (app_running) {
        while (SDL_PollEvent(&e) != 0) {
            // 处理事件
            if (e.type == SDL_EVENT_QUIT) {
                app_running = false;
            } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                // 通知渲染器，窗口大小已更改
                if (renderer) {
                    renderer->signalFramebufferResize();
                }
                // 更新SDL上下文中存储的大小
                if (sdl_context) {
                    int width, height;
                    SDL_GetWindowSizeInPixels(sdl_context->getWindowPtr(),
                                              &width, &height);
                    sdl_context->setSize(width, height);
                }
            }
        }

        // 绘制当前帧
        if (renderer) {
            try {
                renderer->drawFrame();
            } catch (const std::exception& e) {
                spdlog::error("Error during frame rendering: {}", e.what());
                app_running = false;
            }
        }
    }

    // 等待设备完成操作后再退出循环并清理
    if (vulkan_manager && vulkan_manager->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vulkan_manager->getDevice());
    }
}

void TriangleApplication::cleanup() {
    spdlog::info("Cleaning up application...");
    // Cleanup renderer first (depends on VulkanContextManager)
    if (renderer) {
        renderer->cleanup();
        renderer.reset();  // Release unique_ptr
    }
    // Cleanup Vulkan context
    if (vulkan_manager) {
        vulkan_manager->cleanup();
        // Don't delete vulkan_manager if it's a true singleton managed
        // elsewhere
    }
    // Cleanup SDL (handled by SDLContext destructor)
    sdl_context.reset();  // Release unique_ptr

    spdlog::info("Application cleanup complete.");
}