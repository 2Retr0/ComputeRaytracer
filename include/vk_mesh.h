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

struct Mesh {
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    bool load_from_obj(const char *path, const char *baseDirectory = nullptr);
};
