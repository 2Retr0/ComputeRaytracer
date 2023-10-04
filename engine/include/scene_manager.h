#pragma once

#include "scene.h"
#include "hittable.h"
#include "bounding_volume_hierarchy.h"

class SceneManager {
public:
    void init_scene(Scene scene, const std::function<std::shared_ptr<BVHNode>()> &&worldGenerator);

    Scene *get_scene(const std::string &name);

public:
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
};