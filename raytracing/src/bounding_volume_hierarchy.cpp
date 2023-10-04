#include "../include/bounding_volume_hierarchy.h"
#include "../include/axis_aligned_bounding_box.h"
#include "../include/scene.h"

static bool box_compare(const std::shared_ptr<Hittable> &a, const std::shared_ptr<Hittable> &b, int axis) {
    return a->bounding_box().min[axis] < b->bounding_box().min[axis];
}

static float sah_cost(float areaLeft, float areaRight, int numLeft, int numRight) {
    const auto costTraversal = 1.0f;
    const auto costIntersection = 2.15f;

    auto totalArea = areaLeft + areaRight;
    auto probabilityHitLeft = areaLeft / totalArea;
    auto probabilityHitRight = areaRight / totalArea;

    return costTraversal
           + (probabilityHitLeft  * (float) numLeft  * costIntersection)
           + (probabilityHitRight * (float) numRight * costIntersection);
}

struct SplitInfo {
    int axis {-1}, mid {-1};
};

SplitInfo get_best_split(std::vector<std::shared_ptr<Hittable>> objects, int start, int end) {
    assert(end - start > 1);

    SplitInfo bestSplit;
    float bestCost = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; axis++) {
        std::sort(objects.begin() + start, objects.begin() + end, [axis](const auto &a, const auto &b) {
            return box_compare(a, b, axis);
        });

        // FIXME: WHY DOES COMPARING AN INCREASING LEFT BOUND WITH A FIXED RIGHT BOUND IMPROVE PERFORMANCE?!
        auto leftBounds = AABB();
        auto rightBounds = AABB();
        for (int i = start + 1; i < end; i++) rightBounds = AABB(rightBounds, objects[i]->bounding_box());

        for (int mid = start + 1; mid < end; mid++) {
//            auto leftBounds = AABB();
//            auto rightBounds = AABB();
//            for (int i = start; i < mid; i++) leftBounds = AABB(leftBounds, objects[i]->bounding_box());
//            for (int j = mid; j < end; j++) rightBounds = AABB(rightBounds, objects[j]->bounding_box());

            leftBounds = AABB(leftBounds, objects[mid - 1]->bounding_box());

            float cost = sah_cost(leftBounds.area(), rightBounds.area(), mid - start, end - mid);

            if (cost < bestCost) {
                bestCost = cost;
                bestSplit.axis = axis;
                bestSplit.mid = mid;
            }
        }
    }
//    std::cout << "bestSplit: (" << start << ',' << end << ") -> (axis: " << bestSplit.axis << ", mid: " << bestSplit.mid << ", cost: " << bestCost << ")" << std::endl;
    return bestSplit;
}

BVHNode::BVHNode(std::vector<std::shared_ptr<Hittable>> &objects, int start, int end) {
    auto span = end - start;
    if (span == 1) {
        left = right = objects[start];
    } else {
        auto split = get_best_split(objects, start, end);
        // Partition objects based on the calculated best split.
        std::sort(objects.begin() + start, objects.begin() + end, [&](const auto &a, const auto &b) {
            return box_compare(a, b, split.axis);
        });

        left = (split.mid - start == 1) ? objects[start] : std::make_shared<BVHNode>(objects, start, split.mid);
        right = (end - split.mid == 1) ? objects[split.mid] : std::make_shared<BVHNode>(objects, split.mid, end);
    }

    node.aabb = AABB(left->bounding_box(), right->bounding_box());
}

AABB BVHNode::bounding_box() const {
    return node.aabb;
}

Hittable::Type BVHNode::type() const {
    return Hittable::Type::bvhNode;
}

void gpu_serialize_internal(Scene &scene, Hittable *root, uint32_t nextRightNodeIndex, uint32_t nodeIndex) { // NOLINT
    auto type = root->type();
    auto &buffer = scene.get_buffer(type);
    auto &bvh = scene.get_buffer(Hittable::Type::bvhNode);

    if (type != Hittable::Type::bvhNode) {
        auto leaf = BVHNode::GPU_t();
        auto startIndex = (uint32_t) buffer.size();

        // Add children to the buffer. On the GPU, the BVH node will reference the contiguous sequence of children.
        root->gpu_serialize(scene);

        leaf.aabb = root->bounding_box();
        leaf.objectIndex = startIndex;
        leaf.type = type;
        leaf.numChildren = (uint32_t) buffer.size() - startIndex;

        leaf.hitIndex = nextRightNodeIndex;
        leaf.missIndex = nextRightNodeIndex;

        bvh[nodeIndex] = leaf;
    } else {
        auto bvhNode = dynamic_cast<BVHNode *>(root);
        if (bvhNode == nullptr)
            throw std::runtime_error("ERROR: Could not create node when serializing BVH node!");

        auto leftIndex = (uint32_t) bvh.size() + 1;
        auto rightIndex = (uint32_t) bvh.size() + 2;
        bvh.resize(bvh.size() + 3); // Resize bvh to hold current node + children

        bvhNode->node.hitIndex = leftIndex;
        bvhNode->node.missIndex = nextRightNodeIndex;

        bvh[nodeIndex] = bvhNode->node;
        gpu_serialize_internal(scene, bvhNode->left.get(), rightIndex, leftIndex);
        gpu_serialize_internal(scene, bvhNode->right.get(), nextRightNodeIndex, rightIndex);
    }
}

void BVHNode::gpu_serialize(Scene &scene) {
    gpu_serialize_internal(scene, this, BAD_INDEX, 0);
};