#pragma once

#include "sphere.h"

#include <iostream>
#include <random>

#define BAD_INDEX 0xFFFFFFFF

struct GPUBVHNode {
    AABB aabb;
    uint32_t objectBufferIndex {BAD_INDEX};
    uint32_t hitIndex {};
    uint32_t missIndex {};
    float pad {};
};

struct BVHNode {
    BVHNode(std::vector<GPUSphere> &spheres, int start, int end) {
        int axis = std::rand() % 3; // NOLINT
        auto objectSpan = end - start;
        auto comparator = (axis == 0) ? box_x_compare
                        : (axis == 1) ? box_y_compare
                                      : box_z_compare;

        switch (objectSpan) {
            case 1:
                left = right = createNode(spheres, start);
                break;
            case 2:
                if (comparator(spheres[start], spheres[start + 1])) {
                    left = createNode(spheres, start);
                    right = createNode(spheres, start + 1);
                } else {
                    left = createNode(spheres, start + 1);
                    right = createNode(spheres, start);
                }
                break;
            default:
                std::sort(spheres.begin() + start, spheres.begin() + end, comparator);

                auto mid = start + objectSpan / 2;
                left = std::make_shared<BVHNode>(spheres, start, mid);
                right = std::make_shared<BVHNode>(spheres, mid, end);
                break;
        }

        node.aabb = AABB(left->bounding_box(), right->bounding_box());
    }

    BVHNode(AABB aabb, int objectBufferIndex) : node(aabb, objectBufferIndex) {}

public:
    [[nodiscard]] AABB bounding_box() const {
        return node.aabb;
    }

    std::vector<GPUBVHNode> flatten() {
        std::vector<GPUBVHNode> bvh;
        flatten(this, bvh, BAD_INDEX, 0);
        return bvh;
    }

private:
    [[nodiscard]] bool is_leaf() const {
        return this->node.objectBufferIndex != BAD_INDEX;
    }

    static void flatten(BVHNode *root, std::vector<GPUBVHNode> &bvh, uint32_t nextRightNodeIndex, int nodeIndex) { // NOLINT
        if (nodeIndex >= bvh.size()) bvh.resize(nodeIndex + 1);

        if (root->is_leaf()) {
            root->node.hitIndex = nextRightNodeIndex;
            root->node.missIndex = nextRightNodeIndex;

            bvh[nodeIndex] = root->node;
        } else {
            auto leftIndex = 2 * nodeIndex + 1;
            auto rightIndex = 2 * nodeIndex + 2;

            root->node.hitIndex = leftIndex;
            root->node.missIndex = nextRightNodeIndex;

            bvh[nodeIndex] = root->node;
            flatten(root->left.get(), bvh, rightIndex, leftIndex);
            flatten(root->right.get(), bvh, nextRightNodeIndex, rightIndex);
        }
    }

    static std::shared_ptr<BVHNode> createNode(std::vector<GPUSphere> &objects, int index) {
        return std::make_shared<BVHNode>(objects[index].bounding_box(), index);
    }

    static bool box_compare(const GPUSphere &a, const GPUSphere &b, int axis) {
        return a.bounding_box().min[axis] < b.bounding_box().min[axis];
    }

    static bool box_x_compare(const GPUSphere &a, const GPUSphere &b) {
        return box_compare(a, b, 0);
    }

    static bool box_y_compare(const GPUSphere &a, const GPUSphere &b) {
        return box_compare(a, b, 1);
    }

    static bool box_z_compare(const GPUSphere &a, const GPUSphere &b) {
        return box_compare(a, b, 2);
    }

private:
    GPUBVHNode node;
    std::shared_ptr<BVHNode> left {nullptr}, right {nullptr};
};