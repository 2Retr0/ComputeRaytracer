#pragma once

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"

#include <unordered_map>
#include <utility>

class DescriptorAllocator {
public:
    struct PoolSizes {
        std::vector<std::pair<vk::DescriptorType, float>> sizes =
            {
                {vk::DescriptorType::eSampler, 0.5f},
                {vk::DescriptorType::eCombinedImageSampler, 4.f},
                {vk::DescriptorType::eSampledImage, 4.f},
                {vk::DescriptorType::eStorageImage, 1.f},
                {vk::DescriptorType::eUniformTexelBuffer, 1.f},
                {vk::DescriptorType::eStorageTexelBuffer, 1.f},
                {vk::DescriptorType::eUniformBuffer, 2.f},
                {vk::DescriptorType::eStorageBuffer, 2.f},
                {vk::DescriptorType::eUniformBufferDynamic, 1.f},
                {vk::DescriptorType::eStorageBufferDynamic, 1.f},
                {vk::DescriptorType::eInputAttachment, 0.5f}};
    };

    explicit DescriptorAllocator(const vk::raii::Device &device) : device(device) {}

    void reset_pools();

    vk::raii::DescriptorSet allocate(vk::raii::DescriptorSetLayout layout);

public:
    const vk::raii::Device &device;

private:
    vk::raii::DescriptorPool grab_pool();

private:
    vk::DescriptorPool currentPool = nullptr;
    PoolSizes descriptorSizes;
    std::vector<vk::raii::DescriptorPool> usedPools;
    std::vector<vk::raii::DescriptorPool> freePools;
};


class DescriptorLayoutCache {
public:
    struct DescriptorLayoutInfo {
        //good idea to turn this into a inlined array
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        bool operator==(const DescriptorLayoutInfo& other) const;

        size_t hash() const;
    };

    explicit DescriptorLayoutCache(const vk::raii::Device &device) : device(device) {}

    vk::DescriptorSetLayout create_descriptor_layout(vk::DescriptorSetLayoutCreateInfo* info);

public:
    const vk::raii::Device &device;

private:
    struct DescriptorLayoutHash	{
        std::size_t operator()(const DescriptorLayoutInfo& info) const {
            return info.hash();
        }
    };

private:
    std::unordered_map<DescriptorLayoutInfo, vk::raii::DescriptorSetLayout, DescriptorLayoutHash> layoutCache;

};