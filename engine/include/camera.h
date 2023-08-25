#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <SDL.h>

struct CameraProperties {
    glm::vec4 from = glm::vec4(13, 2, 3, 0);
    glm::vec4 at = glm::vec4(0, 0, 0, 0);
    glm::vec4 up = glm::vec4(0, 1, 0, 0);
    glm::vec4 cameraBackward;
    glm::vec4 cameraRight;
    glm::vec4 cameraUp;
    glm::vec4 lowerLeftCorner;
    glm::vec4 horizontal;
    glm::vec4 vertical;
    float lensRadius;
    float iteration;
};

class Camera {
public:
    explicit Camera() {
        auto theta = glm::radians(fovDegrees);
        viewportHeight = 2.0f * glm::tan(theta / 2.0f);
        viewportWidth = aspectRatio * viewportHeight;
        lensRadius = aperture * 0.5f;

        cameraBackward = glm::normalize(from - at);
        calculateProperties();
    }

    void calculateMovement(float tickDelta) {
        auto oldProperties = cameraBackward + from;
        auto moveSensitivity = 0.25f * (tickDelta / 17.0f); // This breaks down past 1000Hz!

        // Handle continuously-held key input for movement.
        const uint8_t *keyStates = SDL_GetKeyboardState({});
        from -= static_cast<float>(keyStates[SDL_SCANCODE_W]) * moveSensitivity * cameraBackward;
        from -= static_cast<float>(keyStates[SDL_SCANCODE_A]) * moveSensitivity * cameraRight;
        from += static_cast<float>(keyStates[SDL_SCANCODE_S]) * moveSensitivity * cameraBackward;
        from += static_cast<float>(keyStates[SDL_SCANCODE_D]) * moveSensitivity * cameraRight;
        from += static_cast<float>(keyStates[SDL_SCANCODE_SPACE]) * moveSensitivity * up;
        from -= static_cast<float>(keyStates[SDL_SCANCODE_LSHIFT]) * moveSensitivity * up;

        // Handle held mouse input for mouse movement->camera movement translation.
        if (SDL_GetMouseState(&mouseX, &mouseY) & SDL_BUTTON_LMASK) {
            SDL_GetRelativeMouseState(&mouseX, &mouseY);

            // Calculate rotation matrix
            float angleX = static_cast<float>(mouseX) * -mouseSensitivity;
            float angleY = static_cast<float>(mouseY) * -mouseSensitivity;
            glm::mat3 rotate = glm::rotate(glm::mat4(1.0f), angleX, up) *
                               glm::rotate(glm::mat4(1.0f), angleY, cameraRight);

            cameraBackward = rotate * cameraBackward;
        }
        calculateProperties();

        auto newProperties = cameraBackward + from;
        if (oldProperties != newProperties) {
            iteration = 1.0;
        } else {
            iteration++;
        }
    }

    void calculateProperties() {
        // Orthonormal basis to describe camera orientation.
        // cameraBackward = glm::normalize(from - at);
        cameraRight = glm::normalize(glm::cross(glm::vec3(up), glm::vec3(cameraBackward)));
        cameraUp = glm::cross(glm::vec3(cameraBackward), glm::vec3(cameraRight));

        horizontal = focusDistance * viewportWidth * cameraRight;
        vertical = focusDistance * viewportHeight * cameraUp;
        lowerLeftCorner = from - horizontal*0.5f - vertical*0.5f - focusDistance*cameraBackward;
    }

    [[nodiscard]] CameraProperties getProperties() const {
        return {
            glm::vec4(from, 0), glm::vec4(at, 0), glm::vec4(up, 0),
            glm::vec4(cameraBackward, 0), glm::vec4(cameraRight, 0), glm::vec4(cameraUp, 0),
            glm::vec4(lowerLeftCorner, 0), glm::vec4(horizontal, 0), glm::vec4(vertical, 0),
            lensRadius, iteration,
        };
    }

public:
    float mouseSensitivity = 0.005f; // Mouse sensitivity is static, move sensitivity is based on frame time!
    int mouseX{}, mouseY{};
    float viewportWidth, viewportHeight;
    glm::vec3 from{13, 2, 3}, at{0, 0, 0}, up{0, 1, 0};
    glm::vec3 cameraBackward{}, cameraRight{}, cameraUp{}, lowerLeftCorner{}, horizontal{}, vertical{};
    float fovDegrees = 90.0f;
    float aspectRatio = 16.0f / 10.0f;
    float aperture = 0.0f;
    float focusDistance = 10.0f; // (from - at).length();
    float lensRadius;
    float iteration = 1.0;
};
