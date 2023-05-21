cmake_minimum_required(VERSION 3.19.0)

project(Wolfie3D)

if (MINGW) 
    include(${CMAKE_CURRENT_SOURCE_DIR}/windows.env.cmake)
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/clang.exe)
    set(CMAKE_CXX_COMPILER ${MINGW_PATH}/bin/clang++.exe)
endif()

execute_process(COMMAND uname OUTPUT_VARIABLE uname)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/shaders)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.cpp
    src/gltf_loader.cpp
    src/stb_image_resize.cpp
    src/tiny_gltf.cpp
    src/vk_mem_alloc.cpp

    src/common/common.cpp
    src/common/common.hpp
    src/common/error.hpp
    src/common/file_utils.cpp
    src/common/file_utils.hpp
    src/common/glm_common.hpp
    src/common/timer.cpp
    src/common/timer.hpp
    src/common/utils.cpp
    src/common/utils.hpp


    src/core/descriptor_allocator.cpp
    src/core/descriptor_allocator.hpp
    src/core/device.cpp
    src/core/device.hpp
    src/core/instance.cpp
    src/core/instance.hpp
    src/core/memory.cpp
    src/core/memory.hpp
    src/core/renderer.cpp
    src/core/renderer.hpp
    src/core/swapchain.cpp
    src/core/swapchain.hpp
    src/core/window.cpp
    src/core/window.hpp

    src/scene_graph/component.cpp
    src/scene_graph/component.hpp
    src/scene_graph/input_event.hpp
    src/scene_graph/node.cpp
    src/scene_graph/node.hpp
    src/scene_graph/scene.cpp
    src/scene_graph/scene.hpp
    src/scene_graph/script.cpp
    src/scene_graph/script.hpp

    src/scene_graph/components/aabb.cpp
    src/scene_graph/components/aabb.hpp
    src/scene_graph/components/camera.cpp
    src/scene_graph/components/camera.hpp
    src/scene_graph/components/image.cpp
    src/scene_graph/components/image.hpp
    src/scene_graph/components/material.cpp
    src/scene_graph/components/material.hpp
    src/scene_graph/components/mesh.cpp
    src/scene_graph/components/mesh.hpp
    src/scene_graph/components/pbr_material.cpp
    src/scene_graph/components/pbr_material.hpp
    src/scene_graph/components/perspective_camera.cpp
    src/scene_graph/components/perspective_camera.hpp
    src/scene_graph/components/sampler.cpp
    src/scene_graph/components/sampler.hpp
    src/scene_graph/components/submesh.cpp
    src/scene_graph/components/submesh.hpp
    src/scene_graph/components/texture.cpp
    src/scene_graph/components/texture.hpp
    src/scene_graph/components/transform.cpp
    src/scene_graph/components/transform.hpp

    src/scene_graph/components/image/stb.cpp
    src/scene_graph/components/image/stb.hpp
    

    src/scene_graph/scripts/free_camera.cpp
    src/scene_graph/scripts/free_camera.hpp
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if (MINGW)
    target_include_directories(${PROJECT_NAME} PUBLIC ${MINGW_PATH}/include)
    target_link_directories(${PROJECT_NAME} PUBLIC ${MINGW_PATH}/lib)
endif()


target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


target_link_libraries(${PROJECT_NAME}
    tinygltf
    glm
    glfw
    stb
    Vulkan::Vulkan
    vma
    gli
    renderdoc
)
