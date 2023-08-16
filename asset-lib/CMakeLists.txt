# Add source to this project's executable.
set(CMAKE_CXX_STANDARD 20)

file(GLOB ASSET_LIB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(assetlib STATIC ${ASSET_LIB_FILES})

target_include_directories(assetlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

target_link_libraries(assetlib PRIVATE nlohmann_json::nlohmann_json lz4::lz4)
