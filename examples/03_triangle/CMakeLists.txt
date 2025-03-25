add_executable(03_triangle main.cpp)
find_package(Vulkan REQUIRED)
target_link_libraries(03_triangle PRIVATE SDL3::SDL3 Vulkan::Vulkan)