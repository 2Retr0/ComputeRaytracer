#pragma once

#include "axis_aligned_bounding_box.h"
#include "glm/geometric.hpp"
#include "glm/vec3.hpp"
#include "hittable.h"
#include "rt_material.h"
#include "scene.h"

#include <cstdint>
#include <utility>

#define PAD 0

class Primitive : public Hittable {
protected:
    explicit Primitive(RTMaterial material) : material(std::move(material)) {}

    void gpu_serialize(Scene &scene) override {
        scene.register_material(material);
    }

protected:
    RTMaterial material;
};

class Sphere : public Primitive {
public:
    struct GPU_t {
        glm::vec3 center;
        float radius;
        glm::vec3 pad0;
        uint32_t materialIndex;
    };

public:
    Sphere(glm::vec3 center, float radius, RTMaterial material) : Primitive(std::move(material)) {
        sphere = GPU_t(center, radius, glm::vec3(PAD));
    }

    [[nodiscard]] AABB bounding_box() const override {
        return {sphere.center - glm::vec3(sphere.radius), sphere.center + glm::vec3(sphere.radius)};
    }

    void gpu_serialize(Scene &scene) override {
        Primitive::gpu_serialize(scene);
        sphere.materialIndex = material.index;
        scene.get_buffer(Hittable::Type::sphere).emplace_back(sphere);
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::sphere;
    }

public:
    GPU_t sphere{};
};


struct Quad : public Primitive {
public:
    struct GPU_t {
        glm::vec3 corner; float d;
        glm::vec3 u;      float pad0;
        glm::vec3 v;      float pad1;
        glm::vec3 normal; float pad2;
        glm::vec3 w;      float pad3;
        glm::vec3 pad4;
        uint32_t materialIndex;
    };

public:
    Quad(glm::vec3 corner, glm::vec3 u, glm::vec3 v, RTMaterial material) : Primitive(std::move(material)) {
        quad = GPU_t(corner, PAD, u, PAD, v);

        auto n = glm::cross(u, v);
        quad.normal = glm::normalize(n);
        quad.d = dot(quad.normal, corner);
        quad.w = n / dot(n, n);
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(quad.corner, quad.corner + quad.u + quad.v).pad();
    }

    void gpu_serialize(Scene &scene) override {
        Primitive::gpu_serialize(scene);
        quad.materialIndex = material.index;
        scene.get_buffer(Hittable::Type::quad).emplace_back(quad);
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::quad;
    }

public:
    GPU_t quad{};
};


struct Box : public HittableList<Quad> {
public:
    Box(glm::vec3 a, glm::vec3 b, const RTMaterial& material) {
        auto min = glm::min(a, b);
        auto max = glm::max(a, b);

        auto dx = glm::vec3(max.x - min.x, 0, 0);
        auto dy = glm::vec3(0, max.y - min.y, 0);
        auto dz = glm::vec3(0, 0, max.z - min.z);

        add(std::make_shared<Quad>(Quad({min.x, min.y, max.z}, dx, dy, material))); // front
        add(std::make_shared<Quad>(Quad({max.x, min.y, max.z},-dz, dy, material))); // right
        add(std::make_shared<Quad>(Quad({max.x, min.y, min.z},-dx, dy, material))); // back
        add(std::make_shared<Quad>(Quad({min.x, min.y, min.z}, dz, dy, material))); // left
        add(std::make_shared<Quad>(Quad({min.x, max.y, max.z}, dx,-dz, material))); // top
        add(std::make_shared<Quad>(Quad({min.x, min.y, min.z}, dx, dz, material))); // bottom
    }
};


struct Tri : public Primitive {
public:
    struct GPU_t {
        glm::vec3 v0; float pad0;
        glm::vec3 v1; float pad1;
        glm::vec3 v2; float pad2;
        glm::vec3 u;  float pad3;
        glm::vec3 v;  float pad4;
        glm::vec3 pad5;
        uint32_t materialIndex;
    };

public:
    Tri(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 u, glm::vec3 v, RTMaterial material) : Primitive(std::move(material)) {
        tri = GPU_t(v0, PAD, v1, PAD, v2, PAD, u, PAD, v, PAD, glm::vec3(PAD), this->material.index);
    }

    [[nodiscard]] AABB bounding_box() const override {
        return AABB(tri.v0, tri.v1, tri.v2).pad();
    }

    void gpu_serialize(Scene &scene) override {
        Primitive::gpu_serialize(scene);
        tri.materialIndex = material.index;
        scene.get_buffer(Hittable::Type::tri).emplace_back(tri);
    }

    [[nodiscard]] Type type() const override {
        return Hittable::Type::tri;
    }

public:
    GPU_t tri{};
};
