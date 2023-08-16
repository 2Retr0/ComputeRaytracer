# Add source to this project's executable.
set(CMAKE_CXX_STANDARD 20)

add_executable(baker "asset_main.cpp")

set_property(TARGET baker PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanTest2>")

target_include_directories(baker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(baker PRIVATE ${Stb_INCLUDE_DIR})

set(LIBRARIES
        tinyobjloader::tinyobjloader
        nlohmann_json::nlohmann_json
        lz4::lz4
        assetlib
)

target_link_libraries(baker PUBLIC ${LIBRARIES})
