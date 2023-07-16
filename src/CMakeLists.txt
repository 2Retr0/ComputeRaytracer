# Add source to this project's executable.
add_executable(${CMAKE_PROJECT_NAME} main.cpp vk_mesh.cpp vk_initializers.cpp vk_engine.cpp vk_textures.cpp)

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanTest2>")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

set(LIBRARIES
        Vulkan::Vulkan
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        imgui::imgui
        tinyobjloader::tinyobjloader
        vk-bootstrap::vk-bootstrap
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBRARIES})

add_dependencies(${CMAKE_PROJECT_NAME} Shaders)
