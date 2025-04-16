cmake_minimum_required(VERSION 3.17)
project(MiniRenderer)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(libs/SDL EXCLUDE_FROM_ALL)
if(APPLE)
    # 注意：请根据您的实际Vulkan SDK安装路径修改以下设置
    message(STATUS "Vulkan SDK path: /Users/avidel/VulkanSDK/1.4.309.0/macOS")
    set(Vulkan_INCLUDE_DIR "/Users/avidel/VulkanSDK/1.4.309.0/macOS/include")
    set(Vulkan_LIBRARY "/Users/avidel/VulkanSDK/1.4.309.0/macOS/lib/libvulkan.dylib")
endif()
find_package(Vulkan REQUIRED)

add_executable(MiniRenderer src/main.cpp)
add_library(minirenderer_includes INTERFACE)
target_include_directories(minirenderer_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL/include
    ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(MiniRenderer PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes)
target_compile_definitions(MiniRenderer PUBLIC SDL_MAIN_USE_CALLBACKS)

add_subdirectory(libs/spdlog)
add_subdirectory(examples)
add_subdirectory(libs/imgui)
add_subdirectory(libs/glm)