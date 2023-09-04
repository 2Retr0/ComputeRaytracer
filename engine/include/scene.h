#pragma once

#include "bounding_volume_hierarchy.h"
#include "glm/fwd.hpp"
#include "glm/gtx/transform.hpp"
#include "hittable.h"
#include "SDL.h"
#include "shapes.h"
#include "vk_engine.h"
#include "vk_types.h"

#include <functional>
#include <vector>

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

    Camera(glm::vec3 position, glm::vec3 at, float aperture, float focusDistance, float fovDegrees) : fovDegrees(fovDegrees) {
        props.position = position;
        props.backward = glm::normalize(props.position - at);
        props.lensRadius = aperture * 0.5f;
        props.focusDistance = focusDistance;
        props.shouldRenderAABB = false;
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
        auto newProperties = props.backward * (props.shouldRenderAABB ? 0.5f : 1.0f) + props.position * fovDegrees + props.focusDistance;
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
    float mouseSensitivity = 0.005f; // Mouse sensitivity is static, move sensitivity is based on frame time!
    GPUCameraData props {};
    float fovDegrees {};
    float aspectRatio = 16.0f / 10.0f;

private:
    int mouseX {}, mouseY {};
    glm::vec3 lastCheckedProperties {};
};

struct GPUSceneData {
    uint32_t sphereCount;
    uint32_t quadCount;
    uint32_t bvhSize;
    float pad;
    GPUCameraData camera;
};

struct Scene {
    Camera camera;
    std::vector<GPUSphere> spheres;
    std::vector<GPUQuad> quads;
    std::vector<GPUBVHNode> bvh;
};

class SceneManager {
public:
    //    void add(const std::string &sceneName, Camera camera, const std::function<void(std::vector<GPUSphere> &, std::vector<GPUQuad> &, std::vector<GPUBVHNode> &)> &sceneConstructor) {
    //        auto scene = Scene(camera);
    //        sceneConstructor(scene.spheres, scene.quads, scene.bvh);
    //
    //        sphereBufferSize = std::max(sphereBufferSize, scene.spheres.size());
    //        quadBufferSize = std::max(quadBufferSize, scene.quads.size());
    //        bvhBufferSize = std::max(bvhBufferSize, scene.bvh.size());
    //        scenes[sceneName] = scene;
    //    }

    void create_scene(const std::string &sceneName, Camera camera, const std::function<std::shared_ptr<BVHNode>()> &&worldGenerator) {
        auto scene = Scene(camera);

        create_scene(scene, worldGenerator().get(), BAD_INDEX, 0);

        sphereBufferSize = std::max(sphereBufferSize, scene.spheres.size());
        quadBufferSize = std::max(quadBufferSize, scene.quads.size());
        bvhBufferSize = std::max(bvhBufferSize, scene.bvh.size());
        scenes[sceneName] = scene;
    }

public:
    std::unordered_map<std::string, Scene> scenes;
    size_t sphereBufferSize {0}, quadBufferSize {0}, bvhBufferSize {0};

private:
    static void create_scene(Scene &scene, Hittable *root, uint32_t nextRightNodeIndex, int nodeIndex) { // NOLINT
        if (nodeIndex >= scene.bvh.size()) scene.bvh.resize(nodeIndex + 1);

        auto bvhNode = dynamic_cast<BVHNode *>(root);

        if (bvhNode == nullptr) { // is leaf
            auto gpuNode = GPUBVHNode();
            auto sphereList = dynamic_cast<HittableList<Sphere> *>(root);
            auto quadList = dynamic_cast<HittableList<Quad> *>(root);

            auto sphere = dynamic_cast<Sphere *>(root);
            auto quad = dynamic_cast<Quad *>(root);

            if (sphereList != nullptr) {
                gpuNode.aabb = sphereList->bounding_box();
                gpuNode.objectBufferIndex = static_cast<uint32_t>(scene.spheres.size());
                gpuNode.type = TYPE_SPHERE;
                gpuNode.numChildren = static_cast<uint32_t>(sphereList->objects.size());

                for (const auto &object : sphereList->objects) {
                    scene.spheres.push_back(*object);
                }
            } else if (quadList != nullptr) {
                gpuNode.aabb = quadList->bounding_box();
                gpuNode.objectBufferIndex = static_cast<uint32_t>(scene.quads.size());
                gpuNode.type = TYPE_QUAD;
                gpuNode.numChildren = static_cast<uint32_t>(quadList->objects.size());

                for (const auto &object : quadList->objects) {
                    scene.quads.push_back(*object);
                }
            } else if (sphere != nullptr) {
                gpuNode.aabb = sphere->bounding_box();
                gpuNode.objectBufferIndex = static_cast<uint32_t>(scene.spheres.size());
                gpuNode.type = TYPE_SPHERE;
                gpuNode.numChildren = 1;

                scene.spheres.push_back(*sphere);
            } else if (quad != nullptr) {
                gpuNode.aabb = quad->bounding_box();
                gpuNode.objectBufferIndex = static_cast<uint32_t>(scene.quads.size());
                gpuNode.type = TYPE_QUAD;
                gpuNode.numChildren = 1;

                scene.quads.push_back(*quad);
            }

            gpuNode.hitIndex = nextRightNodeIndex;
            gpuNode.missIndex = nextRightNodeIndex;

            scene.bvh[nodeIndex] = gpuNode;
        } else {
            auto leftIndex = 2 * nodeIndex + 1;
            auto rightIndex = 2 * nodeIndex + 2;

            bvhNode->node.hitIndex = leftIndex;
            bvhNode->node.missIndex = nextRightNodeIndex;

            scene.bvh[nodeIndex] = bvhNode->node;
            create_scene(scene, bvhNode->left.get(), rightIndex, leftIndex);
            create_scene(scene, bvhNode->right.get(), nextRightNodeIndex, rightIndex);
        }
    }
};