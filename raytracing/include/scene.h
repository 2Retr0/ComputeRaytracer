#pragma once

#include "camera.h"
#include "rt_material.h"

#include <any>
#include <string>
#include <unordered_map>
#include <vector>

#define DEFAULT_BACKGROUND (glm::vec3(-1.0))

struct GPUSceneData {
    glm::vec3 backgroundColor;
    float pad;
    GPUCameraData camera;
};

class Camera;
class Scene {
public:
    Scene() = default;

    Scene(std::string name, Camera camera, glm::vec3 backgroundColor=DEFAULT_BACKGROUND)
        : name(std::move(name)), camera(camera), backgroundColor(backgroundColor) {}

    std::vector<std::any> &get_buffer(int type);

    void register_material(RTMaterial &material);

public:
    std::string name;
    Camera camera;
    glm::vec3 backgroundColor {};
    std::unordered_map<std::string, uint32_t> textures;
    std::unordered_map<RTMaterial, uint32_t> materials;

private:
    std::unordered_map<int, std::vector<std::any>> primitives;
};