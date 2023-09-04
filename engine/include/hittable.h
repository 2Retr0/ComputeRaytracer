#pragma once

#include <memory>

class Hittable {
public:
    [[nodiscard]] virtual AABB bounding_box() const = 0;
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

public:
    std::vector<std::shared_ptr<T>> objects;

private:
    AABB aabb;
};