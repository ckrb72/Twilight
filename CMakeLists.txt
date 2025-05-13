cmake_minimum_required(VERSION 3.13)
project(vk-renderer)
set(CMAKE_CXX_STANDARD 20)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

set(GLFW_BUILD_DOCS OFF BOOL "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/vk-bootstrap)
add_compile_options(-Wno-nullability-completeness)

file(GLOB_RECURSE SRC_CXX_FILES "${SOURCE_DIR}/*.cpp")

find_package(Vulkan REQUIRED)
message(STATUS "Checking Vulkan")
if(NOT Vulkan_LIBRARIES)
    message(FATAL_ERROR "Vulkan not found")
endif()

add_executable(renderer ${SRC_CXX_FILES})
target_link_libraries(renderer Vulkan::Vulkan glfw vk-bootstrap::vk-bootstrap)