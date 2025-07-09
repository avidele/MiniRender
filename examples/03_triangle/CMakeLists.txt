add_executable(03_triangle main.cpp Utils/vulkan_util.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(03_triangle PRIVATE SDL3::SDL3 Vulkan::Vulkan minirenderer_includes spdlog::spdlog glm::glm)

get_target_property(INCLUDES minirenderer_includes INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "minirenderer_includes directories: ${INCLUDES}")

# 设置着色器源目录和目标目录
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# 查找所有着色器文件
file(GLOB SHADER_FILES 
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.spv"
)

# 为每个着色器文件创建复制命令
foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${SHADER_TARGET_DIR}/${SHADER_NAME}")
    
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TARGET_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Copying shader ${SHADER_NAME}"
    )
    
    list(APPEND COPIED_SHADERS ${SHADER_OUTPUT})
endforeach()

# 创建一个自定义目标来管理着色器复制
add_custom_target(copy_shaders ALL DEPENDS ${COPIED_SHADERS})

# 确保在构建主目标之前复制着色器
add_dependencies(03_triangle copy_shaders)