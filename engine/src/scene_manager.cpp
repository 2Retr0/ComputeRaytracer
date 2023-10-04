#include "../include/scene_manager.h"

void SceneManager::init_scene(Scene scene, const std::function<std::shared_ptr<BVHNode>()> &&worldGenerator) {
    std::cout << "\n +---------------------------------------------+\n";
    std::cout << " | Generating scene \"" << scene.name << "\"...                 |\n";
    std::cout << " +---------------------------------------------+" << std::endl;
    worldGenerator()->gpu_serialize(scene);

    scenes[scene.name] = std::make_unique<Scene>(scene);

    auto numMaterials = scene.materials.size(), numTextures = scene.textures.size();
    if (numMaterials > 0) std::cout << "   --- Registered " << scene.materials.size() << " materials...\n";
    if (numTextures > 0)  std::cout << "   --- Registered " << scene.textures.size()  << " textures...\n" << std::endl;
}

Scene *SceneManager::get_scene(const std::string &name) {
    return scenes[name].get();
}