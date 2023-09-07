#pragma once

#include "shapes.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/epsilon.hpp"

#include <cmath>
#include <vector>

struct AABB {
    glm::vec3 min {};
    float pad1 {};
    glm::vec3 max {};
    float pad2 {};

    AABB() = default; // The default AABB is empty, since intervals are empty by default.

    AABB(glm::vec3 a, glm::vec3 b) {
        // Treat the two points a and b as extrema for the bounding box, so we don't require a
        // particular minimum/maximum coordinate order.
        min = glm::vec3(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z));
        max = glm::vec3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z));
    }

    AABB(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        // Treat the two points a and b as extrema for the bounding box, so we don't require a
        // particular minimum/maximum coordinate order.
        min = glm::vec3(fmin(fmin(a.x, b.x), c.x), fmin(fmin(a.y, b.y), c.y), fmin(fmin(a.z, b.z), c.z));
        max = glm::vec3(fmax(fmax(a.x, b.x), c.x), fmax(fmax(a.y, b.y), c.y), fmax(fmax(a.z, b.z), c.z));
    }

    AABB(const AABB &a, const AABB &b) {
        min = glm::vec3(fmin(a.min.x, b.min.x), fmin(a.min.y, b.min.y), fmin(a.min.z, b.min.z));
        max = glm::vec3(fmax(a.max.x, b.max.x), fmax(a.max.y, b.max.y), fmax(a.max.z, b.max.z));
    }

    /**
     * @return An AABB that has no side narrower than some delta, padding if necessary.
     */
    AABB pad() {
        const float delta = 0.0001f;
        auto diff = glm::epsilonEqual(max, min, 0.0001f);

        if (diff.x) expand(0, delta);
        if (diff.y) expand(1, delta);
        if (diff.z) expand(2, delta);

        return {min, max};
    }

private:
    void expand(int axis, float delta) {
        min[axis] -= delta / 2.0f;
        max[axis] += delta / 2.0f;
    }
};