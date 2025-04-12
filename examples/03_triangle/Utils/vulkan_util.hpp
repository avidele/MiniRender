/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
 #pragma once
 #include "SDL3/SDL_init.h"
 #include "SDL3/SDL_video.h"
 #include <memory>
 #include <optional> // For optional queue indices
 // #include <stdexcept> // For error handling
 #include <string>    // Added for shader loading
 #include <vector>
 #include <vulkan/vulkan.h>
 #include <vulkan/vulkan_core.h>
 #define EnableDebug 1
 #if defined(__APPLE__)
 #define VKB_ENABLE_PORTABILITY 1
 #define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
 #endif
 
 static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
 // --- Vertex Data Structure ---
 struct Vertex {
     float pos[2];
     float color[3];
 
     // Describes how to bind vertex data
     static VkVertexInputBindingDescription getBindingDescription() {
         VkVertexInputBindingDescription binding_description{};
         binding_description.binding = 0; // Binding index
         binding_description.stride = sizeof(Vertex); // Size of one vertex entry
         binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Move to next data entry for each vertex
         return binding_description;
     }
 
     // Describes the attributes within a vertex (position, color)
     static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
         std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
 
         // Position attribute
         attribute_descriptions[0].binding = 0; // From which binding the data comes
         attribute_descriptions[0].location = 0; // layout(location = 0) in vertex shader
         attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2
         attribute_descriptions[0].offset = offsetof(Vertex, pos); // Offset within the struct
 
         // Color attribute
         attribute_descriptions[1].binding = 0;
         attribute_descriptions[1].location = 1; // layout(location = 1) in vertex shader
         attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
         attribute_descriptions[1].offset = offsetof(Vertex, color);
 
         return attribute_descriptions;
     }
 };
 
 // --- SDL Window Management ---
 struct SDLWindowDeleter {
     void operator()(SDL_Window* window) const {
         if (window) {
             SDL_DestroyWindow(window);
         }
     }
 };
 
 class SDLContext {
     std::unique_ptr<SDL_Window, SDLWindowDeleter> window;
     int width = 800;
     int height = 600;
 
 public:
     explicit SDLContext(int w = 800, int h = 600) : window(nullptr), width(w), height(h) {}
 
     // Initialize SDL and create the window
     bool init();
     ~SDLContext() {
         window.reset(); // Ensure window is destroyed before SDL_Quit
         SDL_Quit();
     }
 
     SDL_Window* getWindowPtr() const { return window.get(); }
     int getWidth() const { return width; }
     int getHeight() const { return height; }
     // Call this when the window is resized
     void setSize(int w, int h) { width = w; height = h; }
 };
 
 // Forward declaration
 class Renderer;
 
 // --- Core Vulkan Setup Manager (Singleton) ---
 class VulkanContextManager {
     friend class Renderer; // Allow Renderer to access private members for setup
 
 public:
     static VulkanContextManager* getInstance() {
         static VulkanContextManager instance;
         return &instance;
     }
 
     // Prevent copying/moving singletons
     VulkanContextManager(const VulkanContextManager&) = delete;
     VulkanContextManager& operator=(const VulkanContextManager&) = delete;
     VulkanContextManager(VulkanContextManager&&) = delete;
     VulkanContextManager& operator=(VulkanContextManager&&) = delete;
 
     // Initialization and cleanup
     void initVulkan(SDL_Window* window); // Initialize core Vulkan objects
     void cleanup();                      // Clean up all Vulkan resources managed here
 
     // Swapchain handling (public for recreation)
     void createSwapChain();
     void recreateSwapChain(); // Recreate based on current window state
     void cleanupSwapChain();  // Clean up only swapchain related resources
     bool checkDeviceExtensionSupport(VkPhysicalDevice device);
 
     // --- Accessors for Renderer ---
     VkInstance getInstanceHandle() const { return instance; }
     VkPhysicalDevice getPhysicalDevice() const { return physical_device; }
     VkDevice getDevice() const { return device; }
     VkSurfaceKHR getSurface() const { return surface; }
     VkQueue getGraphicsQueue() const { return graphics_queue; }
     VkQueue getPresentQueue() const { return present_queue; }
     VkSwapchainKHR getSwapChain() const { return swap_chain; }
     VkFormat getSwapChainImageFormat() const { return swapchain_image_format; }
     VkExtent2D getSwapChainExtent() const { return swapchain_extent; }
     const std::vector<VkImage>& getSwapChainImages() const { return swapchain_images; }
     const std::vector<VkImageView>& getSwapChainImageViews() const { return swapchain_image_views; }
 
     // --- Utility Functions ---
     uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
     // Helper to create buffers (used by Renderer for vertex buffer)
     void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
     // Helper to copy buffer data (used by Renderer)
     void copyBuffer(VkCommandPool pool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
     // Helper for one-off command buffer execution (e.g., buffer copies)
     VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
     void endSingleTimeCommands(VkCommandPool pool, VkCommandBuffer commandBuffer);
 
 
 #if EnableDebug
     // Debug callback setup
     static VKAPI_ATTR VkBool32 VKAPI_CALL
     debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                   void* pUserData);
     void setupDebugMessenger();
 #endif
 
 private:
     VulkanContextManager() = default;
     ~VulkanContextManager() = default; // Explicit cleanup via cleanup() method
 
     // --- Private Initialization Steps ---
     void createInstance();
     void pickPhysicalDevice();
     void createLogicalDevice();
     void createSurface(SDL_Window* window); // Takes SDL_Window*
     void createImageViews(); // Helper for swapchain creation
 
     // --- Helper Structures and Functions ---
     struct QueueFamilyIndices {
         std::optional<uint32_t> graphics_family;
         std::optional<uint32_t> present_family;
         bool isComplete() const {
             return graphics_family.has_value() && present_family.has_value();
         }
     };
     QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
 
     struct SwapChainSupportDetails {
         VkSurfaceCapabilitiesKHR capabilities;
         std::vector<VkSurfaceFormatKHR> formats;
         std::vector<VkPresentModeKHR> present_modes;
     };
     SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
     bool isDeviceSuitable(VkPhysicalDevice device);
     VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
     VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
     VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
 
     // --- Core Vulkan Objects ---
 #if EnableDebug
     VkDebugUtilsMessengerEXT debug_messenger{VK_NULL_HANDLE};
 #endif
     VkInstance instance{VK_NULL_HANDLE};
     VkSurfaceKHR surface{VK_NULL_HANDLE};
     VkPhysicalDevice physical_device{VK_NULL_HANDLE};
     VkDevice device{VK_NULL_HANDLE}; // Logical device
     VkQueue graphics_queue{VK_NULL_HANDLE};
     VkQueue present_queue{VK_NULL_HANDLE}; // Queue for presenting images
 
     // --- Swapchain Objects ---
     VkSwapchainKHR swap_chain{VK_NULL_HANDLE};
     std::vector<VkImage> swapchain_images;         // Images from the swapchain
     std::vector<VkImageView> swapchain_image_views; // Views into the swapchain images
     VkFormat swapchain_image_format;                // Format of swapchain images
     VkExtent2D swapchain_extent;                    // Resolution of swapchain images
 
     // Keep track of window for swapchain recreation
     SDL_Window* associated_window = nullptr;
 };
 
 
 // --- Rendering Logic ---
 class Renderer {
 public:
     // Takes the context manager it depends on
     explicit Renderer(VulkanContextManager* context);
     ~Renderer(); // Calls cleanup()
 
     // Prevent copying/moving
     Renderer(const Renderer&) = delete;
     Renderer& operator=(const Renderer&) = delete;
     Renderer(Renderer&&) = delete;
     Renderer& operator=(Renderer&&) = delete;
 
     // Setup rendering resources
     void init();
     // Clean up rendering resources
     void cleanup();
 
     // Draw a single frame
     void drawFrame();
 
     // Call this when the window resizes / swapchain becomes invalid
     void handleSwapChainRecreation();
 
     // Signal that the framebuffer needs resizing (called from application event loop)
     void signalFramebufferResize() { framebuffer_resized = true; }
 
 private:
     // --- Initialization Steps ---
     void createRenderPass();
     void createGraphicsPipeline();
     void createFramebuffers();
     void createCommandPool();
     void createVertexBuffer();
     void createCommandBuffers();
     void createSyncObjects(); // Semaphores and fences
 
     // --- Helper Functions ---
     void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
     VkShaderModule createShaderModule(const std::vector<char>& code);
     static std::vector<char> readFile(const std::string& filename); // Utility to load shader SPIR-V files
     void cleanupSwapChainDependents(); // Clean up resources that depend on the swapchain
 
     // --- Member Variables ---
     VulkanContextManager* vulkan_context; // Pointer to the core Vulkan manager
 
     VkRenderPass render_pass{VK_NULL_HANDLE};
     VkPipelineLayout pipeline_layout{VK_NULL_HANDLE}; // Defines uniforms/push constants
     VkPipeline graphics_pipeline{VK_NULL_HANDLE};     // The triangle rendering pipeline
     std::vector<VkFramebuffer> swapchain_framebuffers; // Framebuffers for each swapchain image view
 
     VkCommandPool command_pool{VK_NULL_HANDLE};         // Pool to allocate command buffers from
     std::vector<VkCommandBuffer> command_buffers; // One command buffer per frame in flight
 
     VkBuffer vertex_buffer{VK_NULL_HANDLE};         // GPU buffer for vertex data
     VkDeviceMemory vertex_buffer_memory{VK_NULL_HANDLE}; // Memory backing the vertex buffer
 
     // --- Synchronization ---
     // We use multiple frames in flight to allow CPU to work while GPU renders
     std::vector<VkSemaphore> image_available_semaphores; // Signal when swapchain image is ready
     std::vector<VkSemaphore> render_finished_semaphores; // Signal when rendering is done
     std::vector<VkFence> in_flight_fences; // CPU-GPU sync to prevent submitting too much work
     std::vector<VkFence> images_in_flight; // Track which frame is using which swapchain image
     uint32_t current_frame = 0;            // Index for the current frame in flight
 
     bool framebuffer_resized = false; // Flag set by Application on resize events
 
     // --- Triangle Vertex Data ---
     const std::vector<Vertex> vertices = {
         {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Red vertex at top
         { {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // Green vertex at bottom right
         {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}  // Blue vertex at bottom left
     };
 };
 
 
 // --- Main Application Class ---
 class TriangleApplication {
 public:
     void run(); // Main entry point to start the application
 
 private:
     void initWindow();   // Initialize SDL and the window
     void initVulkan();   // Initialize Vulkan context and renderer
     void mainLoop();     // Run the main event and rendering loop
     void cleanup();      // Clean up all resources
 
     std::unique_ptr<SDLContext> sdl_context;          // Manages the SDL window
     VulkanContextManager* vulkan_manager = nullptr; // Pointer to the singleton Vulkan manager
     std::unique_ptr<Renderer> renderer;             // Manages rendering logic
 
     bool app_running = true; // Controls the main loop execution
 };