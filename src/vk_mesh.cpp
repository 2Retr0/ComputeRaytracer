#include "vk_mesh.h"

#include <iostream>
#include <tiny_obj_loader.h>

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


// Adapted from: https://github.com/tinyobjloader/tinyobjloader
bool Mesh::load_from_obj(const char *path, const char *baseDirectory /*= nullptr*/) {
    // In a `.obj` file, the vertices are not stored together. Instead, it holds a separated arrays of positions,
    // normals, UVs, and Colors, and then an array of faces that points to those. A given `.obj` file also has multiple
    // shapes, as it can hold multiple objects, each of them with separate materials.
    const int faceVertices = 3; // Hardcode loading to triangle (doesn't work for models that haven't been triangulated!)

    tinyobj::attrib_t attributes;               // Contains the vertex arrays of the file.
    std::vector<tinyobj::shape_t> shapes;       // Contains information for each object in the file.
    std::vector<tinyobj::material_t> materials; // Contains information about the material of each shape (unused).

    std::string warning;
    std::string error;
    // We load a single obj file into a single mesh, and all the `.obj` shapes will get merged.
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, path, baseDirectory);

    if (!warning.empty()) {
        std::cout << "WARN: " << warning << std::endl;
    } else if (!error.empty()) {
        // If we have any error, print it to the console, and break the mesh loading. This happens if the file can't be
        // found or is malformed.
        std::cerr << "ERROR: " << error << std::endl;
        return false;
    }

    for (auto &shape : shapes) {
        auto shapeOffset = 0;
        for (auto faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); faceIndex++) {
            for (auto vertexIndex = 0; vertexIndex < faceVertices; vertexIndex++) {
                tinyobj::index_t idx = shape.mesh.indices[shapeOffset + vertexIndex];

                tinyobj::real_t vertex_x = attributes.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vertex_y = attributes.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vertex_z = attributes.vertices[3 * idx.vertex_index + 2];

                tinyobj::real_t normal_x = attributes.normals[3 * idx.normal_index + 0];
                tinyobj::real_t normal_y = attributes.normals[3 * idx.normal_index + 1];
                tinyobj::real_t normal_z = attributes.normals[3 * idx.normal_index + 2];

                tinyobj::real_t ux = attributes.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attributes.texcoords[2 * idx.texcoord_index + 1];

                Vertex newVertex = {};
                newVertex.position = {vertex_x, vertex_y, vertex_z};
                newVertex.normal = {normal_x, normal_y, normal_z};
                newVertex.color = newVertex.normal; // For display purposes only!
                newVertex.uv = {ux, 1 - uy};        // Note: Need to use `1 - uy` for Vulkan format!

                vertices.push_back(newVertex);
            }
            shapeOffset += faceVertices;
        }
    }
    return true;
}
