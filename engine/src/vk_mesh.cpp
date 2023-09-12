#include "vk_mesh.h"
#include "asset_loader.h"
#include "glm/common.hpp"
#include "mesh_asset.h"

#include <iostream>

VertexInputDescription Vertex::get_vertex_description() {
    VertexInputDescription description;
    // We will have just one vertex buffer binding, with a per-vertex rate
    auto mainBinding = vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    description.bindings.push_back(mainBinding);

    // Position at location 0, normal at location 1, color at location 2, and UV at location 3
    auto positionAttribute = vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
    auto normalAttribute = vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal));
    auto colorAttribute = vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));
    auto uvAttribute = vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    description.attributes.push_back(uvAttribute);
    return description;
}


Mesh vkutil::load_mesh_from_asset(const std::string &path) {
    // --- Load File ---
    assets::AssetFile file;
    Mesh mesh;
    bool loaded = assets::load_binaryfile(path.c_str(), file);

    if (!loaded) throw std::runtime_error("ERROR: Failed to load mesh file " + path);

    auto meshInfo = assets::read_mesh_info(&file);

    std::vector<char> vertexByteBuffer(meshInfo.vertexBufferSize);
    std::vector<char> indexByteBuffer(meshInfo.indexBufferSize);

    assets::unpack_mesh(&meshInfo, file.binaryBlob.data(), file.binaryBlob.size(), vertexByteBuffer.data(), indexByteBuffer.data());

    mesh.bounds.extents = glm::vec3(meshInfo.bounds.extents[0], meshInfo.bounds.extents[1], meshInfo.bounds.extents[2]);
    mesh.bounds.origin = glm::vec3(meshInfo.bounds.origin[0], meshInfo.bounds.origin[1], meshInfo.bounds.origin[2]);
    mesh.bounds.radius = meshInfo.bounds.radius;
    mesh.bounds.valid = true;

    mesh.indices.resize(indexByteBuffer.size() / sizeof(uint32_t));
    for (int i = 0; i < mesh.indices.size(); i++) {
        auto *unpackedIndices = (uint32_t *) indexByteBuffer.data();
        mesh.indices[i] = unpackedIndices[i];
    }

    if (meshInfo.vertexFormat == assets::VertexFormat::PNCV_F32) {
        auto *unpackedVertices = (assets::Vertex_f32_PNCV *) vertexByteBuffer.data();

        mesh.vertices.resize(vertexByteBuffer.size() / sizeof(assets::Vertex_f32_PNCV));
        for (int i = 0; i < mesh.vertices.size(); i++) {
            auto &meshVertex = mesh.vertices[i];
            auto &vertex = unpackedVertices[i];
            meshVertex.position = glm::vec3(vertex.position[0], vertex.position[1], vertex.position[2]);
            meshVertex.normal = glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
            meshVertex.color = glm::vec3(vertex.color[0], vertex.color[1], vertex.color[2]);
            meshVertex.uv = glm::vec2(vertex.uv[0], vertex.uv[1]);
        }
    }
    //    else if (meshInfo.vertexFormat == assets::VertexFormat::P32N8C8V16) {
    //        auto *unpackedVertices = (assets::Vertex_P32N8C8V16 *) vertexByteBuffer.data();
    //
    //        vertices.resize(vertexByteBuffer.size() / sizeof(assets::Vertex_P32N8C8V16));
    //        for (int i = 0; i < vertices.size(); i++) {
    //            vertices[i].position.x = vertex.position[0];
    //            vertices[i].position.y = vertex.position[1];
    //            vertices[i].position.z = vertex.position[2];
    //
    //            vertices[i].pack_normal(glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]));
    //
    //            vertices[i].color.x = vertex.color[0]; // / 255.f;
    //            vertices[i].color.y = vertex.color[1]; // / 255.f;
    //            vertices[i].color.z = vertex.color[2]; // / 255.f;
    //
    //            vertices[i].uv.x = vertex.uv[0];
    //            vertices[i].uv.y = vertex.uv[1];
    //        }
    //    }

    std::cout << "   --- Loaded mesh file \"" << path
              << "\" : (vert=" << mesh.vertices.size() << ", tri=" << mesh.indices.size() / 3 << ')' << std::endl;
    return std::move(mesh);
}
