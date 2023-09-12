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
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static VertexInputDescription get_vertex_description();
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
    AllocatedBuffer vertexBuffer;
    RenderBounds bounds;

//    bool load_mesh_from_asset(const char *path);
};

class VulkanEngine;
namespace vkutil {
    Mesh load_mesh_from_asset(const std::string &path);
} // namespace vkutil
