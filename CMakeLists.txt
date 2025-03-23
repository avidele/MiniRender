cmake_minimum_required(VERSION 3.17)
project(MiniRenderer)
add_subdirectory(libs/SDL EXCLUDE_FROM_ALL)
find_package(Vulkan REQUIRED)

add_executable(MiniRenderer src/main.cpp)
target_link_libraries(MiniRenderer SDL3::SDL3 Vulkan::Vulkan)
target_compile_definitions(MiniRenderer PUBLIC SDL_MAIN_USE_CALLBACKS)

add_subdirectory(examples)
add_subdirectory(libs/imgui)