#pragma once

#include "sphere.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
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

    AABB(const AABB &a, const AABB &b) {
        min = glm::vec3(fmin(a.min.x, b.min.x), fmin(a.min.y, b.min.y), fmin(a.min.z, b.min.z));
        max = glm::vec3(fmax(a.max.x, b.max.x), fmax(a.max.y, b.max.y), fmax(a.max.z, b.max.z));
    }
};