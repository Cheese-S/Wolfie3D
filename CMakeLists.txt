if (WIN32)
    include(./windows.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
endif()
if (UNIX)
    include(./unix.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
endif()
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

# Specify the minimum version for CMake

cmake_minimum_required(VERSION 3.11.0)

set(NAME Wolfie3D)

# Specify generators and compilers
message(STATUS "using ${CMAKE_GENERATOR}")

if (CMAKE_GENERATOR STREQUAL "Ninja" AND WIN32)
    if (NOT MINGW_PATH)
        message(FATAL_ERROR "MINGW_PATH not set in .env.cmake")
    endif()
    set(USE_MINGW "True")
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/clang.exe)
    set(CMAKE_CXX_COMPILER ${MINGW_PATH}/bin/clang++.exe)
endif()

project(${NAME} VERSION 0.1.0)



# Find Vulkan
if (DEFINED VULKAN_SDK_PATH)
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include") # 1.1 Make sure this include path is correct
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib") # 1.2 Make sure lib path is correct
    set(Vulkan_FOUND "True")
else()
    find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
    message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set(Vulkan_LIBRARIES "$ENV{Vulkan_SDK}/Lib")
    endif()
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()

# Find GLFW
if (DEFINED GLFW_PATH)
    message(STATUS "Using GLFW path specified in .env")
    set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
    if (MSVC)
        set(GLFW_LIB "${GLFW_PATH}/lib-vc2019")
    elseif (CMAKE_GENERATOR STREQUAL "Ninja")
        message(STATUS "USING NINJA")
        set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64") 
    endif()
else()
    find_package(glfw3 3.3 REQUIRED)
    set(GLFW_LIB glfw)
    message(STATUS "Found GLFW")
endif()
if (NOT GLFW_LIB)
	message(FATAL_ERROR "Could not find glfw library!")
else()
	message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

# Find tinygltf
if (NOT TINYGLTF_PATH)
    message(STATUS "TINYGLTF_PATH not specified in .env.cmake, using external/tinygltf")
    set(TINYGLTF_PATH external/tinygltf)
endif()


if (NOT VMA_PATH)
    message(STATUS "VMA_PATH not specified in .enc.cmake using external/vulkan_memory_allocator")
    set(VMA_PATH external/vulkan_memory_allocator)
endif()

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")


if (WIN32)
    message(STATUS "CREATING BUILD FOR WINDOWS")
    
    if (USE_MINGW)
        target_include_directories(${PROJECT_NAME} PUBLIC 
            ${MINGW_PATH}/include
        )
        target_link_directories(${PROJECT_NAME} PUBLIC
            ${MINGW_PATH}/lib
        )
    endif()

    target_include_directories(${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
        ${GLM_PATH}
        ${TINYGLTF_PATH}
        ${VMA_PATH}
    )

    target_link_directories(${PROJECT_NAME} PUBLIC
        ${Vulkan_LIBRARIES}
        ${GLFW_LIB}
    )

    target_link_libraries(${PROJECT_NAME} glfw3 vulkan-1)
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${TINYGLTF_PATH}
        ${VMA_PATH}
    )
    target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
endif()

# Build shaders

set( ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

file(GLOB SHADERS ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag ${SHADER_DIR}/*.comp ${SHADER_DIR}/*.geom ${SHADER_DIR}/*.tesc ${SHADER_DIR}/*.tese ${SHADER_DIR}/*.mesh ${SHADER_DIR}/*.task ${SHADER_DIR}/*.rgen ${SHADER_DIR}/*.rchit ${SHADER_DIR}/*.rmiss)

foreach(SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    add_custom_command(OUTPUT ${SHADER_OUTPUT_DIR}/${FILENAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT_DIR}/${FILENAME}.spv
        DEPENDS ${SHADER}
        COMMENT "Compiling ${FILENAME}")
list(APPEND SPV_SHADERS ${SHADER_OUTPUT_DIR}/${FILENAME}.spv)
endForeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})

add_dependencies(Wolfie3D shaders)