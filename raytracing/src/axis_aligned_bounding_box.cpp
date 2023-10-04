#include "axis_aligned_bounding_box.h"

AABB::AABB(glm::vec3 a, glm::vec3 b) {
    min = glm::min(a, b);
    max = glm::max(a, b);
}

AABB::AABB(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    min = glm::min(glm::min(a, b), c);
    max = glm::max(glm::max(a, b), c);
}

AABB::AABB(const AABB &a, const AABB &b) {
    min = glm::min(a.min, b.min);
    max = glm::max(a.max, b.max);
}

AABB AABB::pad() {
    const float delta = 0.0001f;
    auto diff = glm::epsilonEqual(max, min, 0.0001f);

    if (diff.x) expand(0, delta);
    if (diff.y) expand(1, delta);
    if (diff.z) expand(2, delta);

    return {min, max};
}

[[nodiscard]] float AABB::area() const {
    auto lengths = max - min;
    return 2.0f * (lengths.x*lengths.y + lengths.y*lengths.z + lengths.x*lengths.z);
}

void AABB::expand(int axis, float delta) {
    min[axis] -= delta / 2.0f;
    max[axis] += delta / 2.0f;
}
