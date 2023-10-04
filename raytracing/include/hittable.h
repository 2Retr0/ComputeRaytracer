#pragma once

#include "axis_aligned_bounding_box.h"
#include "scene.h"

#include <algorithm>
#include <any>
#include <iostream>
#include <memory>

class Hittable {
public:
    enum Type : uint32_t {
        sphere  = 1,
        quad    = 2,
        tri     = 4,
        bvhNode = 8,
    };

    [[nodiscard]] virtual AABB bounding_box() const = 0;

    [[nodiscard]] virtual Type type() const = 0;

    virtual void gpu_serialize(Scene &scene) = 0;
};

template<typename T> requires std::is_base_of_v<Hittable, T>
class HittableList : public Hittable {
public:
    HittableList() = default;

    explicit HittableList(const std::shared_ptr<T> &object) {
        add(object);
    }

    void add(const std::shared_ptr<T> &object) {
        objects.push_back(object);
        aabb = AABB(aabb, object->bounding_box());
    }

    [[nodiscard]] AABB bounding_box() const override {
        return aabb;
    }

    void gpu_serialize(Scene &scene) override {
        for (const auto &object : objects)
            object->gpu_serialize(scene);
    }

    [[nodiscard]] Type type() const override {
        return objects.empty() ? throw std::runtime_error("ERROR: Cannot serialize an empty HittableList!") : objects.front()->type();
    }

public:
    std::vector<std::shared_ptr<Hittable>> objects;

private:
    AABB aabb;
};