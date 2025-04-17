add_executable(04_triangle_spin main.cpp Utils/vulkan_util.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(04_triangle_spin PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes spdlog::spdlog glm::glm)

get_target_property(INCLUDES minirenderer_includes INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "minirenderer_includes directories: ${INCLUDES}")

# 定义 shaders 源目录
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

set(SHADER_DESTINATION_DIR $<TARGET_FILE_DIR:04_triangle_spin>/shaders)

add_custom_command(
    TARGET 04_triangle_spin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DESTINATION_DIR}
    COMMENT "Creating shader destination directory"
)

add_custom_command(
    TARGET 04_triangle_spin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SOURCE_DIR} ${SHADER_DESTINATION_DIR}
    COMMENT "Copying shaders to build directory"
    VERBATIM 
)