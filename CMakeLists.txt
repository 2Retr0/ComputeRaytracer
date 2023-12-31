﻿# CMakeList.txt : CMake project for vulkan_guide, include source and define project specific logic here.
cmake_minimum_required(VERSION 3.8)
project("ComputeRaytracer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g -fsanitize=address" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto -fopenmp" CACHE STRING "")

find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(PACKAGES sdl2 vk-bootstrap tinyobjloader imgui VulkanMemoryAllocator unofficial-vulkan-memory-allocator-hpp glm nlohmann_json lz4)
foreach (package IN LISTS PACKAGES)
    find_package(${package} CONFIG REQUIRED)
endforeach ()

add_subdirectory(asset-baker)
add_subdirectory(asset-lib)
add_subdirectory(raytracing)
add_subdirectory(engine)

# Find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

# Iterate each shader
foreach (GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    # Execute glslang command to compile that specific shader
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
