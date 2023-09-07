#pragma once

#include "shapes.h"

#include <iostream>
#include <random>

#define BAD_INDEX 0xFFFFFFFF

struct GPUBVHNode {
    AABB aabb;
    uint32_t objectBufferIndex {BAD_INDEX};
    uint32_t hitIndex {};
    uint32_t missIndex {};
    float pad1{};
    uint32_t type {};
    uint32_t numChildren {};
    glm::vec2 pad2 {};
};

class BVHNode : public Hittable {
public:
    BVHNode(std::vector<std::shared_ptr<Hittable>> &objects, int start, int end) {
        int axis = std::rand() % 3; // NOLINT
        auto objectSpan = end - start;
        auto comparator = [=](const auto &a, const auto &b) {
            return (axis == 0) ? box_compare(a, b, 0) : (axis == 1) ? box_compare(a, b, 1) : box_compare(a, b, 2);
        };

        switch (objectSpan) {
            case 1:
                left = right = objects[start];
                break;
            case 2:
                if (comparator(objects[start], objects[start + 1])) {
                    left = objects[start];
                    right = objects[start + 1];
                } else {
                    left = objects[start + 1];
                    right = objects[start];
                }
                break;
            default:
                std::sort(objects.begin() + start, objects.begin() + end, comparator);

                auto mid = start + objectSpan / 2;
                left = std::make_shared<BVHNode>(objects, start, mid);
                right = std::make_shared<BVHNode>(objects, mid, end);
                break;
        }

        node.aabb = AABB(left->bounding_box(), right->bounding_box());
    }

    [[nodiscard]] AABB bounding_box() const override {
        return node.aabb;
    }

//    std::vector<GPUBVHNode> flatten() {
//        std::vector<GPUBVHNode> bvh;
//        flatten(this, bvh, BAD_INDEX, 0);
//        return bvh;
//    }

public:
    GPUBVHNode node;
    std::shared_ptr<Hittable> left {nullptr}, right {nullptr};

private:
//    [[nodiscard]] bool is_leaf() const {
//        return this->node.objectBufferIndex != BAD_INDEX;
//    }
//
//    static void flatten(BVHNode *root, std::vector<GPUBVHNode> &bvh, uint32_t nextRightNodeIndex, int nodeIndex) { // NOLINT
//        if (nodeIndex >= bvh.size()) bvh.resize(nodeIndex + 1);
//
//        if (root->is_leaf()) {
//            root->node.hitIndex = nextRightNodeIndex;
//            root->node.missIndex = nextRightNodeIndex;
//
//            bvh[nodeIndex] = root->node;
//        } else {
//            auto leftIndex = 2 * nodeIndex + 1;
//            auto rightIndex = 2 * nodeIndex + 2;
//
//            root->node.hitIndex = leftIndex;
//            root->node.missIndex = nextRightNodeIndex;
//
//            bvh[nodeIndex] = root->node;
//            flatten(root->left.get(), bvh, rightIndex, leftIndex);
//            flatten(root->right.get(), bvh, nextRightNodeIndex, rightIndex);
//        }
//    }

    static bool box_compare(const std::shared_ptr<Hittable> &a, const std::shared_ptr<Hittable> &b, int axis) {
        return a->bounding_box().min[axis] < b->bounding_box().min[axis];
    }
};