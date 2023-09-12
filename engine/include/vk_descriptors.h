#pragma once

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"

#include <unordered_map>
#include <utility>

namespace vkutil {
    struct Descriptor {
        vk::raii::DescriptorSet set = nullptr;
        vk::DescriptorSetLayout layout;
    };

    class DescriptorAllocator {
    public:
        struct PoolSizes {
            std::vector<std::pair<vk::DescriptorType, float>> sizes = {
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

        DescriptorAllocator() = default;
        explicit DescriptorAllocator(const vk::raii::Device *device) : device(device) {}

        void reset_pools();

        vk::raii::DescriptorSet allocate(vk::DescriptorSetLayout layout);

    public:
        const vk::raii::Device *device {};

    private:
        vk::raii::DescriptorPool grab_pool();

    private:
        vk::DescriptorPool currentPool = VK_NULL_HANDLE;
        PoolSizes descriptorSizes;
        std::vector<vk::raii::DescriptorPool> usedPools;
        std::vector<vk::raii::DescriptorPool> freePools;
    };


    class DescriptorLayoutCache {
    public:
        struct DescriptorLayoutInfo {
            // Good idea to turn this into an inlined array
            std::vector<vk::DescriptorSetLayoutBinding> bindings;

            bool operator==(const DescriptorLayoutInfo &other) const;

            [[nodiscard]] size_t hash() const;
        };

        DescriptorLayoutCache() = default;
        explicit DescriptorLayoutCache(const vk::raii::Device *device) : device(device) {}

        vk::DescriptorSetLayout create_descriptor_layout(vk::DescriptorSetLayoutCreateInfo *info);

    public:
        const vk::raii::Device *device {};

    private:
        struct DescriptorLayoutHash {
            std::size_t operator()(const DescriptorLayoutInfo &info) const {
                return info.hash();
            }
        };

    private:
        std::unordered_map<DescriptorLayoutInfo, std::unique_ptr<vk::raii::DescriptorSetLayout>, DescriptorLayoutHash> layoutCache;
    };

    class DescriptorBuilder {
    public:
        static DescriptorBuilder begin(DescriptorLayoutCache *layoutCache, DescriptorAllocator *allocator);

        DescriptorBuilder &bind_buffer(uint32_t binding, vk::DescriptorBufferInfo *bufferInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);
        DescriptorBuilder &bind_image(uint32_t binding, vk::DescriptorImageInfo *imageInfo, vk::DescriptorType type, vk::ShaderStageFlags stageFlags);

        //    bool build(vk::raii::DescriptorSet &set, vk::DescriptorSetLayout &layout);
        std::unique_ptr<Descriptor> build();

    private:
        std::vector<vk::WriteDescriptorSet> writes;
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        DescriptorLayoutCache *cache;
        DescriptorAllocator *allocator;
    };
} // namespace vkutil