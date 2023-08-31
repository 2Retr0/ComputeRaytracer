#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <cstdint>

#define MAT_LAMBERTIAN 1
#define MAT_METAL      2
#define MAT_DIELECTRIC 3

struct GPUMaterial {
    glm::vec4 albedo;
    glm::vec4 type;

    GPUMaterial(glm::vec3 albedo, float fuzziness, uint32_t type) : albedo(glm::vec4(albedo, fuzziness)), type(glm::vec4(type, 0, 0, 0)) {}
};

struct GPUSphere {
    glm::vec4 center;
    GPUMaterial material;

    GPUSphere(glm::vec3 center, float radius, GPUMaterial material) : center(glm::vec4(center, radius)), material(material) {}

    [[nodiscard]] AABB bounding_box() const {
        auto center = glm::vec3(this->center);
        auto radius = this->center[3];
        return {center - glm::vec3(radius), center + glm::vec3(radius)};
    }
};
