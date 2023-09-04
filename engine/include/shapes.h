#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/geometric.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "hittable.h"

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
};

class Sphere : public Hittable {
public:
    Sphere(glm::vec3 center, float radius, GPUMaterial material) {
        sphere = GPUSphere(center, radius, material);
    }

    operator GPUSphere() { // NOLINT
        return sphere;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return {sphere.center - glm::vec3(sphere.radius), sphere.center + glm::vec3(sphere.radius)};
    }

private:
    GPUSphere sphere{};
};

struct GPUQuad {
    glm::vec3 corner;
    float pad1;
    glm::vec3 u;
    float pad2;
    glm::vec3 v;
    float pad3;
    glm::vec3 normal;
    float d;
    glm::vec3 w;
    float pad4;
    GPUMaterial material;
};

struct Quad : public Hittable {
public:
    Quad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, GPUMaterial material) {
        quad = GPUQuad(corner, 0, u, 0, v);

        auto n = glm::cross(u, v);
        quad.normal = glm::normalize(n);
        quad.d = dot(quad.normal, corner);
        quad.w = n / dot(n, n);
        quad.material = material;
    }

    operator GPUQuad() { // NOLINT
        return quad;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(quad.corner, quad.corner + quad.u + quad.v).pad();
    }

private:
    GPUQuad quad{};
};
