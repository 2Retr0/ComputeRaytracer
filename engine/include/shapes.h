#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <cstdint>

#define MAT_LAMBERTIAN 1
#define MAT_METAL      2
#define MAT_DIELECTRIC 3

struct GPUMaterial {
    glm::vec3 albedo;
    float fuzziness;
    uint32_t type;
    glm::vec3 pad;
};

struct GPUSphere {
    glm::vec3 center;
    float radius;
    GPUMaterial material;

    [[nodiscard]] AABB bounding_box() const {
        return {center - glm::vec3(radius), center + glm::vec3(radius)};
    }
};
