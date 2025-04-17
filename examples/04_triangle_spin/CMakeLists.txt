add_executable(04_triangle_spin main.cpp Utils/vulkan_util.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(04_triangle_spin PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes spdlog::spdlog glm::glm)

get_target_property(INCLUDES minirenderer_includes INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "minirenderer_includes directories: ${INCLUDES}")