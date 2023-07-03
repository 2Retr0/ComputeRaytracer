#pragma once

#include "vk_types.h"
#include "glm/vec3.hpp"

#include <vector>

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

struct Mesh {
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    bool load_from_obj(const char *path, const char *baseDirectory = nullptr);
};
