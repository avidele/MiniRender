file(GLOB_RECURSE IMGUI_FILES ./*.h ./*.cpp)
add_library(imgui STATIC ${IMGUI_FILES})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui PRIVATE SDL3::SDL3)