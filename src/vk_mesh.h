#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>
#include <iostream>
#include "tiny_obj_loader.h"

struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    static VertexInputDescription get_vertex_description();
};

VertexInputDescription Vertex::get_vertex_description() {
    VertexInputDescription description;

    // We will have just one vertex buffer binding, with a per-vertex rate
    VkVertexInputBindingDescription mainBinding = {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    description.bindings.push_back(mainBinding);

    VkVertexInputAttributeDescription positionAttribute = {
            .location = 0, // Position will be stored at Location 0
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
    };

    VkVertexInputAttributeDescription normalAttribute = {
            .location = 1, // Normal will be stored at Location 1
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
    };

    VkVertexInputAttributeDescription colorAttribute = {
            .location = 2, // Normal will be stored at Location 2
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
    };

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    return description;
}


struct Mesh {
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    bool load_from_obj(const char* filename);
};

// Adapted from: https://github.com/tinyobjloader/tinyobjloader
bool Mesh::load_from_obj(const char *filename) {
    // In a `.obj` file, the vertices are not stored together. Instead, it holds a separated arrays of positions,
    // normals, UVs, and Colors, and then an array of faces that points to those. A given `.obj` file also has multiple
    // shapes, as it can hold multiple objects, each of them with separate materials.
    const int faceVertices = 3; // Hardcode loading to triangle (doesn't work for models that haven't been triangulated!)

    tinyobj::attrib_t attributes;               // Contains the vertex arrays of the file.
    std::vector<tinyobj::shape_t> shapes;       // Contains information for each separate object in the file.
    std::vector<tinyobj::material_t> materials; // Contains information about the material of each shape (unused).

    std::string warning;
    std::string error;

    // We load a single obj file into a single mesh, and all the `.obj` shapes will get merged.
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, filename, nullptr);

    if (!warning.empty()) {
        std::cout << "WARN: " << warning << std::endl;
    } else if (!error.empty()) {
        // If we have any error, print it to the console, and break the mesh loading. This happens if the file can't be
        // found or is malformed.
        std::cerr << error << std::endl;
        return false;
    }

    size_t shapeOffset = 0;
    for (auto& shape : shapes) {
        for (size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); faceIndex++) {
            for (size_t vertexIndex = 0; vertexIndex < faceVertices; vertexIndex++) {
                tinyobj::index_t idx = shape.mesh.indices[shapeOffset + vertexIndex];

                tinyobj::real_t vertex_x = attributes.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vertex_y = attributes.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vertex_z = attributes.vertices[3 * idx.vertex_index + 2];

                tinyobj::real_t normal_x = attributes.normals[3 * idx.normal_index + 0];
                tinyobj::real_t normal_y = attributes.normals[3 * idx.normal_index + 1];
                tinyobj::real_t normal_z = attributes.normals[3 * idx.normal_index + 2];

                Vertex newVertex{};
                newVertex.position = {vertex_x, vertex_y, vertex_z};
                newVertex.normal = {normal_x, normal_y, normal_z};
                newVertex.color = newVertex.normal; // For display purposes only!

                vertices.push_back(newVertex);
            }
            shapeOffset += faceVertices;
        }
    }
    return true;
}
