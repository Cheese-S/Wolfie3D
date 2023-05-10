cmake_minimum_required(VERSION 3.19.0)

project(external LANGUAGES C CXX)

# Read user defined dirs
include(${CMAKE_CURRENT_SOURCE_DIR}/build.env.cmake)

# vulkan
find_package(Vulkan REQUIRED GLOBAL)

# tinygltf
add_library(tinygltf INTERFACE)
set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
target_sources(tinygltf INTERFACE ${TINYGLTF_DIR}/tiny_gltf.h ${TINYGLTF_DIR}/json.hpp)
target_include_directories(tinygltf INTERFACE ${TINYGLTF_DIR})

# glm
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/glm)


# glfw
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_INSTALL OFF)
option(BUILD_SHARED_LIBS ON)
add_subdirectory(glfw)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE 
${CMAKE_CURRENT_SOURCE_DIR}/stb
)

# vma
add_library(vma INTERFACE)
set(VMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)
target_sources(vma INTERFACE ${VMA_DIR}/vk_mem_alloc.h)
target_include_directories(vma INTERFACE ${VMA_DIR})
target_link_libraries(vma INTERFACE Vulkan::Vulkan)

# libktx
add_library(libktx INTERFACE)
target_include_directories(libktx INTERFACE
    ${LIBKTX_DIR}/include
)
target_link_libraries(libktx INTERFACE
    ${LIBKTX_DIR}/lib/ktx.lib
)
    