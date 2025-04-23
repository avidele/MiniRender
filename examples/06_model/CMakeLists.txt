add_executable(06_model main.cpp Utils/vulkan_util.cpp)
find_package(Vulkan REQUIRED)
# Link tinyobjloader
target_link_libraries(06_model PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes spdlog::spdlog glm::glm)

get_target_property(INCLUDES minirenderer_includes INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "minirenderer_includes directories: ${INCLUDES}")

# 定义 shaders 源目录
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

set(SHADER_DESTINATION_DIR $<TARGET_FILE_DIR:06_model>/shaders)

add_custom_command(
    TARGET 06_model POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DESTINATION_DIR}
    COMMENT "Creating shader destination directory"
)

add_custom_command(
    TARGET 06_model POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SOURCE_DIR} ${SHADER_DESTINATION_DIR}
    COMMENT "Copying shaders to build directory"
    VERBATIM 
)

# Copy texture image to the build directory - This might be removed later if textures are only in Obj/
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Image/texture.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Image)

# Copy Obj directory (models and textures) to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Obj DESTINATION ${CMAKE_CURRENT_BINARY_DIR})