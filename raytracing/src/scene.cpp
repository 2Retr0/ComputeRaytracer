#include "../include/scene.h"

#include <vector>

std::vector<std::any> &Scene::get_buffer(int type) {
    return primitives[type];
}

void Scene::register_material(RTMaterial &material) {
    // Register texture first so the material texture index field can be set before it is hashed
    auto textureIndex = BAD_INDEX;
    auto textureName = material.texture;
    if (!material.texture.empty()) {
        if (textures.contains(textureName)) {
            textureIndex = textures[textureName];
        } else {
            textureIndex = (uint32_t) textures.size();
            textures[textureName] = textureIndex;
        }
    }
    material.material.textureIndex = textureIndex; // Initialize material texture index field

    auto materialIndex = (uint32_t) materials.size();
    if (materials.contains(material))
        materialIndex = materials[material];
    else
        materials[material] = materialIndex;
    material.index = materialIndex; // Initialize material index field
}