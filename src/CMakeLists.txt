
# Add source to this project's executable.
add_executable(VulkanTest2
    main.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.h)


set_property(TARGET VulkanTest2 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(VulkanTest2 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(VulkanTest2 vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(VulkanTest2 Vulkan::Vulkan sdl2)

add_dependencies(VulkanTest2 Shaders)
