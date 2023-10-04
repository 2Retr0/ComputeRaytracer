#pragma once

#include "axis_aligned_bounding_box.h"
#include "scene.h"
#include "glm/vec2.hpp"
#include "hittable.h"

#include <iostream>
#include <random>

#define BAD_INDEX 0xFFFFFFFF

class BVHNode : public Hittable {
public:
    struct GPU_t {
        AABB aabb;
        uint32_t objectIndex {BAD_INDEX};
        uint32_t hitIndex {};
        uint32_t missIndex {};
        float pad1 {};
        Hittable::Type type {};
        uint32_t numChildren {};
        glm::vec2 pad2 {};
    };

public:
    BVHNode(std::vector<std::shared_ptr<Hittable>> &objects, int start, int end);

    [[nodiscard]] AABB bounding_box() const override;

    [[nodiscard]] Type type() const override;

    void gpu_serialize(Scene &scene) override;

public:
    GPU_t node;
    std::shared_ptr<Hittable> left {nullptr}, right {nullptr};
};