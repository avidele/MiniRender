add_executable(03_triangle main.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(03_triangle PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes spdlog::spdlog)

get_target_property(INCLUDES minirenderer_includes INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "minirenderer_includes directories: ${INCLUDES}")