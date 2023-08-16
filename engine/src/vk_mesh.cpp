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
    auto normalAttribute = vk::VertexInputAttributeDescription(1, 0, vk::Format::eR8G8Unorm, offsetof(Vertex, oct_normal));
    auto colorAttribute = vk::VertexInputAttributeDescription(2, 0, vk::Format::eR8G8B8Unorm, offsetof(Vertex, color));
    auto uvAttribute = vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    description.attributes.push_back(uvAttribute);
    return description;
}


// Source: https://github.com/vblanco20-1/vulkan-guide/blob/engine/extra-engine/vk_mesh.cpp
glm::vec2 oct_normal_wrap(glm::vec2 v) {
    glm::vec2 wrap;
    wrap.x = (1.0f - glm::abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f);
    wrap.y = (1.0f - glm::abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f);
    return wrap;
}


// Source: https://github.com/vblanco20-1/vulkan-guide/blob/engine/extra-engine/vk_mesh.cpp
glm::vec2 oct_normal_encode(glm::vec3 normal) {
    normal /= (glm::abs(normal.x) + glm::abs(normal.y) + glm::abs(normal.z));

    glm::vec2 wrapped = oct_normal_wrap(normal);
    glm::vec2 result;
    result.x = normal.z >= 0.0f ? normal.x : wrapped.x;
    result.y = normal.z >= 0.0f ? normal.y : wrapped.y;

    result.x = result.x * 0.5f + 0.5f;
    result.y = result.y * 0.5f + 0.5f;

    return result;
}


void Vertex::pack_normal(glm::vec3 normal) {
    glm::vec2 oct = oct_normal_encode(normal);

    oct_normal.x = uint8_t(oct.x * 255);
    oct_normal.y = uint8_t(oct.y * 255);
}


void Vertex::pack_color(glm::vec3 c) {
    color.r = static_cast<uint8_t>(c.x * 255);
    color.g = static_cast<uint8_t>(c.y * 255);
    color.b = static_cast<uint8_t>(c.z * 255);
}


bool Mesh::load_mesh_from_asset(const char *path) {
    // --- Load File ---
    assets::AssetFile file;
    bool loaded = assets::load_binaryfile(path, file);

    if (!loaded) {
        std::cout << "ERROR: Failed to load texture file " << path << std::endl;
        return false;
    }

    auto meshInfo = assets::read_mesh_info(&file);

    std::vector<char> vertexByteBuffer;
    std::vector<char> indexByteBuffer;

    vertexByteBuffer.resize(meshInfo.vertexBufferSize);
    indexByteBuffer.resize(meshInfo.indexBufferSize);

    assets::unpack_mesh(&meshInfo, file.binaryBlob.data(), file.binaryBlob.size(), vertexByteBuffer.data(), indexByteBuffer.data());

    bounds.extents.x = meshInfo.bounds.extents[0];
    bounds.extents.y = meshInfo.bounds.extents[1];
    bounds.extents.z = meshInfo.bounds.extents[2];

    bounds.origin.x = meshInfo.bounds.origin[0];
    bounds.origin.y = meshInfo.bounds.origin[1];
    bounds.origin.z = meshInfo.bounds.origin[2];

    bounds.radius = meshInfo.bounds.radius;
    bounds.valid = true;

    vertices.clear();
    indices.clear();

    indices.resize(indexByteBuffer.size() / sizeof(uint32_t));
    for (int i = 0; i < indices.size(); i++) {
        auto *unpackedIndices = (uint32_t *) indexByteBuffer.data();
        indices[i] = unpackedIndices[i];
    }

    if (meshInfo.vertexFormat == assets::VertexFormat::PNCV_F32) {
        auto *unpackedVertices = (assets::Vertex_f32_PNCV *) vertexByteBuffer.data();

        vertices.resize(vertexByteBuffer.size() / sizeof(assets::Vertex_f32_PNCV));
        for (int i = 0; i < vertices.size(); i++) {
            vertices[i].position.x = unpackedVertices[i].position[0];
            vertices[i].position.y = unpackedVertices[i].position[1];
            vertices[i].position.z = unpackedVertices[i].position[2];

            auto normal = glm::vec3(unpackedVertices[i].normal[0], unpackedVertices[i].normal[1], unpackedVertices[i].normal[2]);
            vertices[i].pack_normal(normal);
            vertices[i].pack_color(glm::vec3 {unpackedVertices[i].color[0], unpackedVertices[i].color[1], unpackedVertices[i].color[2]});

            vertices[i].uv.x = unpackedVertices[i].uv[0];
            vertices[i].uv.y = unpackedVertices[i].uv[1];
        }
    } else if (meshInfo.vertexFormat == assets::VertexFormat::P32N8C8V16) {
        auto *unpackedVertices = (assets::Vertex_P32N8C8V16 *) vertexByteBuffer.data();

        vertices.resize(vertexByteBuffer.size() / sizeof(assets::Vertex_P32N8C8V16));
        for (int i = 0; i < vertices.size(); i++) {
            vertices[i].position.x = unpackedVertices[i].position[0];
            vertices[i].position.y = unpackedVertices[i].position[1];
            vertices[i].position.z = unpackedVertices[i].position[2];

            vertices[i].pack_normal(glm::vec3(unpackedVertices[i].normal[0], unpackedVertices[i].normal[1], unpackedVertices[i].normal[2]));

            vertices[i].color.x = unpackedVertices[i].color[0]; // / 255.f;
            vertices[i].color.y = unpackedVertices[i].color[1]; // / 255.f;
            vertices[i].color.z = unpackedVertices[i].color[2]; // / 255.f;

            vertices[i].uv.x = unpackedVertices[i].uv[0];
            vertices[i].uv.y = unpackedVertices[i].uv[1];
        }
    }

    std::cout << "Successfully loaded mesh file " << path
              << " : vert=" << vertices.size() << ", tri=" << indices.size() / 3 << std::endl;
    return true;
}
