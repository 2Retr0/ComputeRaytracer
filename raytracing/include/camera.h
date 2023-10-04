#pragma once

#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/vec3.hpp"
#include "SDL2/SDL.h"

struct GPUCameraData {
    glm::vec3 position;
    bool shouldRenderAABB;
    glm::vec3 backward;
    float lensRadius;
    glm::vec3 right;
    float focusDistance;
    glm::vec3 up;
    float iteration;
    glm::vec3 horizontal;
    float seed;
    glm::vec3 vertical;
    float pad;
};

class Camera {
public:
    Camera() = default;

    Camera(glm::vec3 position, glm::vec3 at, float fovDegrees=70.0, float aspectRatio=1.0, float aperture=1.0f / 45.0f, float focusDistance=10.0f);

    void calculateMovement(float tickDelta);

    void calculateProperties();

public:
    float mouseSensitivity = 0.005f; // Mouse sensitivity is static, move sensitivity is based on frame time!
    GPUCameraData props {};
    float fovDegrees {};
    float aspectRatio = 16.0f / 9.0f;

private:
    int mouseX {}, mouseY {};
    glm::vec3 lastCheckedProperties {};
};