#pragma once

#include "glm/gtc/epsilon.hpp"
#include "glm/vec3.hpp"

#include <cmath>
#include <vector>

struct AABB {
    glm::vec3 min {};
    float pad1 {};
    glm::vec3 max {};
    float pad2 {};

    AABB() = default; // The default AABB is empty, since intervals are empty by default.

    /**
     * Treat the two points a and b as extrema for the bounding box, so we don't require a
     * particular minimum/maximum coordinate order.
     */
    AABB(glm::vec3 a, glm::vec3 b);

    AABB(glm::vec3 a, glm::vec3 b, glm::vec3 c);

    AABB(const AABB &a, const AABB &b);

    /**
     * @return An AABB that has no side narrower than some delta, padding if necessary.
     */
    AABB pad();

    [[nodiscard]] float area() const;

private:
    void expand(int axis, float delta);
};