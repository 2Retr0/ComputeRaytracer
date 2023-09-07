#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/geometric.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "hittable.h"

#include <cstdint>

#define MAT_LAMBERTIAN    0
#define MAT_METAL         1
#define MAT_DIELECTRIC    2
#define MAT_DIFFUSE_LIGHT 3

#define PAD 0

struct GPUMaterial {
    glm::vec3 albedo;
    float fuzziness;
    uint32_t type;
    glm::vec3 pad0;
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
    glm::vec3 corner; float d;
    glm::vec3 u;      float pad0;
    glm::vec3 v;      float pad1;
    glm::vec3 normal; float pad2;
    glm::vec3 w;      float pad3;
    GPUMaterial material;
};

struct Quad : public Hittable {
public:
    Quad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, GPUMaterial material) {
        quad = GPUQuad(corner, PAD, u, PAD, v);

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


struct GPUTri {
    glm::vec3 v0; float pad0;
    glm::vec3 v1; float pad1;
    glm::vec3 v2; float pad2;
    glm::vec3 u;  float pad3;
    glm::vec3 v;  float pad4;
    GPUMaterial material;
};


struct Tri : public Hittable {
public:
    Tri(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 u, glm::vec3 v, GPUMaterial material) {
        tri = GPUTri(v0, PAD, v1, PAD, v2, PAD, u, PAD, v, PAD, material);
    }

    operator GPUTri() { // NOLINT
        return tri;
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(tri.v0, tri.v1, tri.v2).pad();
    }

private:
    GPUTri tri{};
};
