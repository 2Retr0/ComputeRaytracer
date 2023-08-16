# Add source to this project's executable.
set(CMAKE_CXX_STANDARD 20)

file(GLOB ENGINE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${CMAKE_PROJECT_NAME} ${ENGINE_FILES})

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanTest2>")

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

set(LIBRARIES
        Vulkan::Vulkan
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        GPUOpen::VulkanMemoryAllocator
        unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
        glm::glm
        imgui::imgui
        vk-bootstrap::vk-bootstrap
        assetlib
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBRARIES})

add_dependencies(${CMAKE_PROJECT_NAME} Shaders)
