#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <any>

class Hittable {
public:
    enum Type {
        sphere  = 1,
        quad    = 2,
        tri     = 4,
        nonLeaf = 8,
    };

    [[nodiscard]] virtual AABB bounding_box() const = 0;

    [[nodiscard]] virtual std::vector<std::any> gpu_serialize() const = 0;

    [[nodiscard]] virtual Type type() const = 0;
};

template<typename T, std::enable_if<std::is_base_of<Hittable, T>::value>::type* = nullptr>
class HittableList : public Hittable {
public:
    HittableList() = default;

    explicit HittableList(const std::shared_ptr<T> &object) {
        add(object);
    }

    void clear() {
        objects.clear();
    }

    void add(const std::shared_ptr<T> &object) {
        objects.push_back(object);
        aabb = AABB(aabb, object->bounding_box());
    }

    [[nodiscard]] AABB bounding_box() const override {
        return aabb;
    }

    [[nodiscard]] std::vector<std::any> gpu_serialize() const override {
        std::vector<std::any> buffer;
        for (const auto &object : objects) {
            auto serialized = object->gpu_serialize();
            buffer.insert(buffer.end(), serialized.begin(), serialized.end());
        }
        return buffer;
    }

    [[nodiscard]] Type type() const override {
        return objects.empty() ? Hittable::Type::nonLeaf : objects.front()->type();
    }

public:
    std::vector<std::shared_ptr<Hittable>> objects;

private:
    AABB aabb;
};