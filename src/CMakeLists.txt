# Add source to this project's executable.
add_executable(${CMAKE_PROJECT_NAME} main.cpp vk_mesh.cpp vk_initializers.cpp vk_engine.cpp)

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanTest2>")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(${CMAKE_PROJECT_NAME} Vulkan::Vulkan sdl2)

add_dependencies(${CMAKE_PROJECT_NAME} Shaders)
