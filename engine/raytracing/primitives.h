#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/geometric.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "hittable.h"

#include <cstdint>

enum MaterialType {
    lambertian   = 1,
    metal        = 2,
    dielectric   = 4,
    diffuseLight = 8,
};

#define PAD 0

struct GPUMaterial {
    glm::vec3 albedo;
    float fuzziness;
    uint32_t type;
    glm::vec3 pad0;
};

class Sphere : public Hittable {
public:
    struct GPU_t {
        glm::vec3 center;
        float radius;
        GPUMaterial material;
    };

public:
    Sphere(glm::vec3 center, float radius, GPUMaterial material) {
        sphere = GPU_t{center, radius, material};
    }

    operator GPU_t() { // NOLINT
        return sphere;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return {sphere.center - glm::vec3(sphere.radius), sphere.center + glm::vec3(sphere.radius)};
    }

    [[nodiscard]] std::vector<std::any> gpu_serialize() const override {
        return {sphere};
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::sphere;
    }

private:
    GPU_t sphere{};
};


struct Quad : public Hittable {
public:
    struct GPU_t {
        glm::vec3 corner; float d;
        glm::vec3 u;      float pad0;
        glm::vec3 v;      float pad1;
        glm::vec3 normal; float pad2;
        glm::vec3 w;      float pad3;
        GPUMaterial material;
    };

public:
    Quad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, GPUMaterial material) {
        quad = GPU_t{corner, PAD, u, PAD, v};

        auto n = glm::cross(u, v);
        quad.normal = glm::normalize(n);
        quad.d = dot(quad.normal, corner);
        quad.w = n / dot(n, n);
        quad.material = material;
    }

    operator GPU_t() { // NOLINT
        return quad;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(quad.corner, quad.corner + quad.u + quad.v).pad();
    }

    [[nodiscard]] std::vector<std::any> gpu_serialize() const override {
        return {quad};
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::quad;
    }

private:
    GPU_t quad{};
};


struct Tri : public Hittable {
public:
    struct GPU_t {
        glm::vec3 v0; float pad0;
        glm::vec3 v1; float pad1;
        glm::vec3 v2; float pad2;
        glm::vec3 u;  float pad3;
        glm::vec3 v;  float pad4;
        GPUMaterial material;
    };

public:
    Tri(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 u, glm::vec3 v, GPUMaterial material) {
        tri = GPU_t{v0, PAD, v1, PAD, v2, PAD, u, PAD, v, PAD, material};
    }

    operator GPU_t() { // NOLINT
        return tri;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(tri.v0, tri.v1, tri.v2).pad();
    }

    [[nodiscard]] std::vector<std::any> gpu_serialize() const override {
        return {tri};
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::tri;
    }

private:
    GPU_t tri{};
};
