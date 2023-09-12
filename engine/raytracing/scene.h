#pragma once

#include "bounding_volume_hierarchy.h"
#include "camera.h"
#include "hittable.h"
#include "primitives.h"
#include "vk_types.h"

#include <any>
#include <functional>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

#define DEFAULT_BACKGROUND (glm::vec3(-1.0))

struct GPUSceneData {
    glm::vec3 backgroundColor;
    float pad;
    GPUCameraData camera;
};

class Scene {
public:
    Scene() = default;

    Scene(std::string name, Camera camera, glm::vec3 backgroundColor=DEFAULT_BACKGROUND)
        : name(std::move(name)), camera(camera), backgroundColor(backgroundColor) {}

    std::vector<std::any> &get_buffer(Hittable::Type type) {
        return primitives[type];
    }

public:
    std::string name;
    Camera camera;
    glm::vec3 backgroundColor{};
    std::vector<BVHNode::GPU_t> bvh;

private:
    std::unordered_map<Hittable::Type, std::vector<std::any>> primitives;
};

class SceneManager {
public:
    void init_scene(Scene scene, const std::function<std::shared_ptr<BVHNode>()> &&worldGenerator) {
        create_scene(scene, worldGenerator().get(), BAD_INDEX, 0);

        bvhBufferSize = std::max(bvhBufferSize, scene.bvh.size());
        scenes[scene.name] = std::make_unique<Scene>(scene);
    }

    Scene get_scene(const std::string &name) {
        return *scenes[name];
    }

public:
    size_t bvhBufferSize {0};
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;

private:
    static void create_scene(Scene &scene, Hittable *root, uint32_t nextRightNodeIndex, int nodeIndex) { // NOLINT
        if (nodeIndex >= scene.bvh.size()) scene.bvh.resize(nodeIndex + 1);

        auto type = root->type();

        if (type != Hittable::Type::nonLeaf) {
            auto node = BVHNode::GPU_t();
            auto &buffer = scene.get_buffer(type);
            auto children = root->gpu_serialize();

            node.aabb = root->bounding_box();
            node.objectBufferIndex = static_cast<uint32_t>(buffer.size());
            node.type = type;
            node.numChildren = static_cast<uint32_t>(children.size());

            // Add children to the buffer. On the GPU, the BVH node will reference the contiguous sequence of children.
            buffer.insert(buffer.end(), children.begin(), children.end());

            node.hitIndex = nextRightNodeIndex;
            node.missIndex = nextRightNodeIndex;

            scene.bvh[nodeIndex] = node;
        } else {
            auto bvhNode = dynamic_cast<BVHNode *>(root);
            if (bvhNode == nullptr)
                throw std::runtime_error("ERROR: Unsupported Hittable type encountered when creating scene BVH!");

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