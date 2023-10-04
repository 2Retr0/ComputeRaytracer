#pragma once

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"

#include <iostream>
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

            [[nodiscard]] uint64_t hash() const;
        };

        DescriptorLayoutCache() = default;
        explicit DescriptorLayoutCache(const vk::raii::Device *device) : device(device) {}

        vk::DescriptorSetLayout create_descriptor_layout(vk::DescriptorSetLayoutCreateInfo *info);

    public:
        const vk::raii::Device *device {};

    private:
        struct DescriptorLayoutHash {
            uint64_t operator()(const DescriptorLayoutInfo &info) const {
                return info.hash();
            }
        };

    private:
        std::unordered_map<DescriptorLayoutInfo, std::unique_ptr<vk::raii::DescriptorSetLayout>, DescriptorLayoutHash> layoutCache;
    };

    class DescriptorBuilder {
    public:
        static DescriptorBuilder begin(DescriptorLayoutCache *layoutCache, DescriptorAllocator *allocator);

        template<typename T> requires std::is_same_v<T, vk::DescriptorImageInfo> || std::is_same_v<T, vk::DescriptorBufferInfo>
        DescriptorBuilder &bind(uint32_t binding, T *infos, vk::DescriptorType type, vk::ShaderStageFlags stageFlags, size_t numInfos=1) {
            if constexpr (std::is_same_v<T, vk::DescriptorBufferInfo>) {
                for (int i = 0; i < numInfos; i++) {
                    if (infos[i].range == 0) {
//                        std::cerr << "WARN: vk::DescriptorBufferInfo::range must be non-zero!" << std::endl;
                        infos[i].range = 1;
                    }
                }
            }

            // --- Descriptor Layout Binding Creation ---
            auto newBinding = vk::DescriptorSetLayoutBinding(binding, type, (uint32_t) numInfos, stageFlags);
            bindings.push_back(newBinding);

            // --- Descriptor Write Creation ---
            // clang-format off
            auto newWrite = vk::WriteDescriptorSet({}, binding)
                .setDescriptorCount((uint32_t) numInfos)
                .setDescriptorType(type);
            // clang-format on
            if constexpr (std::is_same_v<T, vk::DescriptorBufferInfo>)
                newWrite.setPBufferInfo(infos);
            else if constexpr (std::is_same_v<T, vk::DescriptorImageInfo>)
                newWrite.setPImageInfo(infos);

            writes.push_back(newWrite);
            return *this;
        }

        //    bool build(vk::raii::DescriptorSet &set, vk::DescriptorSetLayout &layout);
        std::unique_ptr<Descriptor> build();

    private:
        std::vector<vk::WriteDescriptorSet> writes;
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        DescriptorLayoutCache *cache;
        DescriptorAllocator *allocator;
    };
} // namespace vkutil