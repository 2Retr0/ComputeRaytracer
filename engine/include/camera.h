#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <SDL.h>

struct CameraProperties {
    glm::vec3 position;
    float pad;
    glm::vec3 backward;
    float lensRadius;
    glm::vec3 right;
    float focusDistance;
    glm::vec3 up;
    float iteration;
    glm::vec3 horizontal;
    float seed;
    glm::vec3 vertical;
    uint32_t sphereCount;
    uint32_t bvhCount;
};

class Camera {
public:
    explicit Camera() {
        auto at = glm::vec3(0, 0, -0.25);
        auto aperture = 0.0f;

        props.position = glm::vec3(10, 1.5, 2);
        props.backward = glm::normalize(props.position - at);
        props.lensRadius = aperture * 0.5f;
        props.focusDistance = 10.0f;
        calculateProperties();
    }

    void calculateMovement(float tickDelta) {
        auto moveSensitivity = 0.25f * (tickDelta / 17.0f); // This breaks down past 1000Hz!

        // Handle continuously-held key input for movement.
        const uint8_t *keyStates = SDL_GetKeyboardState({});
        props.position -= static_cast<float>(keyStates[SDL_SCANCODE_W]) * moveSensitivity * props.backward;
        props.position -= static_cast<float>(keyStates[SDL_SCANCODE_A]) * moveSensitivity * props.right;
        props.position += static_cast<float>(keyStates[SDL_SCANCODE_S]) * moveSensitivity * props.backward;
        props.position += static_cast<float>(keyStates[SDL_SCANCODE_D]) * moveSensitivity * props.right;
        props.position += static_cast<float>(keyStates[SDL_SCANCODE_SPACE]) * moveSensitivity * props.up;
        props.position -= static_cast<float>(keyStates[SDL_SCANCODE_LSHIFT]) * moveSensitivity * props.up;

        // Handle held mouse input for mouse movement->camera movement translation.
        if (SDL_GetMouseState(&mouseX, &mouseY) & SDL_BUTTON_LMASK) {
            SDL_GetRelativeMouseState(&mouseX, &mouseY);

            // Calculate rotation matrix
            float angleX = static_cast<float>(mouseX) * -mouseSensitivity;
            float angleY = static_cast<float>(mouseY) * -mouseSensitivity;
            glm::mat3 rotate = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(0, 1, 0)) *
                               glm::rotate(glm::mat4(1.0f), angleY, props.right);

            props.backward = rotate * props.backward;
        }
    }

    void calculateProperties() {
        auto newProperties = props.backward + props.position + fovDegrees;
        if (lastCheckedProperties == newProperties) {
            props.iteration++;
            return;
        }

        auto theta = glm::radians(fovDegrees);
        auto viewportHeight = 2.0f * glm::tan(theta / 2.0f);
        auto viewportWidth = aspectRatio * viewportHeight;

        lastCheckedProperties = newProperties;

        // Orthonormal basis to describe camera orientation.
        props.right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), props.backward));
        props.up = glm::cross(props.backward, props.right);

        props.horizontal = props.focusDistance * viewportWidth * props.right;
        props.vertical = props.focusDistance * viewportHeight * props.up;

        props.iteration = 1;
    }

public:
    const float mouseSensitivity = 0.005f; // Mouse sensitivity is static, move sensitivity is based on frame time!
    CameraProperties props {};
    float fovDegrees = 30.0f;
    float aspectRatio = 16.0f/10.0f;

private:
    int mouseX{}, mouseY{};
    glm::vec3 lastCheckedProperties{};
};
