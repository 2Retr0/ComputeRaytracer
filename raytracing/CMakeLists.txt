# Add source to this project's executable.
set(CMAKE_CXX_STANDARD 20)

file(GLOB RAYTRACING_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(raytracing STATIC ${RAYTRACING_FILES})

target_include_directories(raytracing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

target_link_libraries(raytracing PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    glm::glm
)
