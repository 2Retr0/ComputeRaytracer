#include "asset_loader.h"
#include "mesh_asset.h"
#include "texture_asset.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tiny_obj_loader.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

bool convert_image(const fs::path &input, const fs::path &output) {
    int width, height, channels;
    auto *pixels = stbi_load((const char *) input.u8string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        std::cout << "ERROR: Failed to load texture file " << input << '!' << std::endl;
        return false;
    }

    assets::TextureInfo textureInfo;
    textureInfo.textureSize = width * height * 4;
    textureInfo.pixelSize[0] = width;
    textureInfo.pixelSize[1] = height;
    textureInfo.textureFormat = assets::TextureFormat::RGBA8;
    textureInfo.originalFile = input.string();

    assets::AssetFile newImage = assets::pack_texture(&textureInfo, pixels);
    stbi_image_free(pixels);
    save_binaryfile(output.string().c_str(), newImage);

    return true;
}


void pack_vertex(assets::Vertex_f32_PNCV &newVertex, tinyobj::real_t vertex_x, tinyobj::real_t vertex_y, tinyobj::real_t vertex_z, tinyobj::real_t normal_x, tinyobj::real_t normal_y, tinyobj::real_t normal_z, tinyobj::real_t ux, tinyobj::real_t uy) {
    newVertex.position[0] = vertex_x;
    newVertex.position[1] = vertex_y;
    newVertex.position[2] = vertex_z;

    newVertex.normal[0] = normal_x;
    newVertex.normal[1] = normal_y;
    newVertex.normal[2] = normal_z;

    newVertex.uv[0] = ux;
    newVertex.uv[1] = 1 - uy;
}


void pack_vertex(assets::Vertex_P32N8C8V16 &newVertex, tinyobj::real_t vertex_x, tinyobj::real_t vertex_y, tinyobj::real_t vertex_z, tinyobj::real_t normal_x, tinyobj::real_t normal_y, tinyobj::real_t normal_z, tinyobj::real_t ux, tinyobj::real_t uy) {
    newVertex.position[0] = vertex_x;
    newVertex.position[1] = vertex_y;
    newVertex.position[2] = vertex_z;

    newVertex.normal[0] = uint8_t(((normal_x + 1.0) / 2.0) * 255);
    newVertex.normal[1] = uint8_t(((normal_y + 1.0) / 2.0) * 255);
    newVertex.normal[2] = uint8_t(((normal_z + 1.0) / 2.0) * 255);

    newVertex.uv[0] = ux;
    newVertex.uv[1] = 1 - uy;
}


// Adapted from: https://github.com/tinyobjloader/tinyobjloader
template<typename V>
void extract_mesh_from_obj(std::vector<tinyobj::shape_t> &shapes, tinyobj::attrib_t &attributes, std::vector<uint32_t> &indices, std::vector<V> &vertices) {
    // In a `.obj` file, the vertices are not stored together. Instead, it holds a separated arrays of positions,
    // normals, UVs, and Colors, and then an array of faces that points to those. A given `.obj` file also has multiple
    // shapes, as it can hold multiple objects, each of them with separate materials.
    const int FACE_VERTICES = 3; // Hardcode loading to triangle (doesn't work for models that haven't been triangulated!)

    for (auto &shape : shapes) {
        auto shapeOffset = 0;
        for (auto faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); faceIndex++) {
            for (auto vertexIndex = 0; vertexIndex < FACE_VERTICES; vertexIndex++) {
                tinyobj::index_t idx = shape.mesh.indices[shapeOffset + vertexIndex];

                tinyobj::real_t vertex_x = attributes.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vertex_y = attributes.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vertex_z = attributes.vertices[3 * idx.vertex_index + 2];

                tinyobj::real_t normal_x = attributes.normals[3 * idx.normal_index + 0];
                tinyobj::real_t normal_y = attributes.normals[3 * idx.normal_index + 1];
                tinyobj::real_t normal_z = attributes.normals[3 * idx.normal_index + 2];

                tinyobj::real_t ux = attributes.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attributes.texcoords[2 * idx.texcoord_index + 1];

                V newVertex = {};
                pack_vertex(newVertex, vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z, ux, uy);

                indices.push_back(vertices.size());
                vertices.push_back(newVertex);
            }
            shapeOffset += FACE_VERTICES;
        }
    }
}


bool convert_mesh(const fs::path &input, const fs::path &output) {
    tinyobj::attrib_t attributes;               // Contains the vertex arrays of the file.
    std::vector<tinyobj::shape_t> shapes;       // Contains information for each object in the file.
    std::vector<tinyobj::material_t> materials; // Contains information about the material of each shape (unused).

    std::string warning;
    std::string error;

    // --- Load Mesh File ---
    auto loadTimeStart = std::chrono::high_resolution_clock::now();

    // We load a single obj file into a single mesh, and all the `.obj` shapes will get merged.
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, input.string().c_str(), input.parent_path().string().c_str());

    auto loadTimeDiff = std::chrono::high_resolution_clock::now() - loadTimeStart;
    std::cout << "Took " << std::chrono::duration_cast<std::chrono::nanoseconds>(loadTimeDiff).count() / static_cast<uint64_t>(1E6) << "ms to load!" << std::endl;

    if (!warning.empty()) {
        std::cout << "WARN: " << warning << std::endl;
    } else if (!error.empty()) {
        // If we have any error, print it to the console, and break the mesh loading. This happens if the file can't be
        // found or is malformed.
        std::cerr << "ERROR: " << error << std::endl;
        return false;
    }

    using VertexFormat = assets::Vertex_f32_PNCV;
    auto VertexFormatEnum = assets::VertexFormat::PNCV_F32;

    std::vector<VertexFormat> vertices;
    std::vector<uint32_t> indices;
    extract_mesh_from_obj(shapes, attributes, indices, vertices);

    assets::MeshInfo meshInfo;
    meshInfo.vertexFormat = VertexFormatEnum;
    meshInfo.vertexBufferSize = vertices.size() * sizeof(VertexFormat);
    meshInfo.indexBufferSize = indices.size() * sizeof(uint32_t);
    meshInfo.indexSize = sizeof(uint32_t);
    meshInfo.originalFile = input.string();
    meshInfo.bounds = assets::calculate_bounds(vertices.data(), vertices.size());

    // --- Pack Mesh File ---
    auto packTimeStart = std::chrono::high_resolution_clock::now();

    assets::AssetFile newFile = assets::pack_mesh(&meshInfo, (char *) vertices.data(), (char *) indices.data());

    auto packTimeDiff = std::chrono::high_resolution_clock::now() - packTimeStart;
    std::cout << "Took " << std::chrono::duration_cast<std::chrono::nanoseconds>(packTimeDiff).count() / static_cast<uint64_t>(1E6) << "ms to compress!" << std::endl;

    save_binaryfile(output.string().c_str(), newFile);

    return true;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Asset path is required as input!";
        return -1;
    } else {
        fs::path path {argv[1]};

        std::cout << "Loading asset directory at " << path << std::endl;
        for (auto &file : fs::recursive_directory_iterator(path)) {
            auto newPath = file.path();
            if (file.path().extension() == ".png") {
                std::cout << " -- Found texture file " << file << std::endl;

                newPath.replace_extension(".tx");
                convert_image(file.path(), newPath);
            }
            if (file.path().extension() == ".obj") {
                std::cout << " -- Found mesh file " << file << std::endl;

                newPath.replace_extension(".mesh");
                convert_mesh(file.path(), newPath);
            }
        }
    }

    exit(0);
}