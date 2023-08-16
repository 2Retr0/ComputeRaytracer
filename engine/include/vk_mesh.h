#pragma once

#include "vk_types.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

#include <vector>

struct VertexInputDescription {
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributes;
    vk::PipelineVertexInputStateCreateFlags flags;
};

struct Vertex {
    glm::vec3 position;
//    glm::vec3 normal;
    glm::vec<2, uint8_t> oct_normal; // color
    glm::vec<3, uint8_t> color;
    glm::vec2 uv;

    static VertexInputDescription get_vertex_description();
    void pack_normal(glm::vec3 normal);
    void pack_color(glm::vec3 color);
};

struct RenderBounds {
    glm::vec3 origin;
    float radius;
    glm::vec3 extents;
    bool valid;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    AllocatedBuffer<Vertex> vertexBuffer;
    AllocatedBuffer<uint32_t> indexBuffer;

    RenderBounds bounds;

    bool load_mesh_from_asset(const char *path);
};
