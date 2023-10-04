#include "vk_descriptors.h"

#include <iostream>

namespace vkutil {
    vk::raii::DescriptorPool create_pool(const vk::raii::Device *device, const DescriptorAllocator::PoolSizes &poolSizes, int count, vk::DescriptorPoolCreateFlagBits flags) {
        std::vector<vk::DescriptorPoolSize> sizes;
        sizes.reserve(poolSizes.sizes.size());

        for (auto &[type, ratio] : poolSizes.sizes) {
            sizes.emplace_back(type, static_cast<uint32_t>(ratio * static_cast<float>(count)));
        }
        auto poolInfo = vk::DescriptorPoolCreateInfo(flags, count, static_cast<uint32_t>(sizes.size()), sizes.data());

        return device->createDescriptorPool(poolInfo);
    }

    vk::raii::DescriptorPool DescriptorAllocator::grab_pool() {
        if (!freePools.empty()) { // Reusable pools are available
            auto pool = std::move(freePools.back());
            freePools.pop_back();
            return pool;
        } else { // No pools available, so create a new one
            return create_pool(device, descriptorSizes, 1000, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        }
    }


    vk::raii::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
        //initialize the currentPool handle if it's null
        if (!currentPool) {
            usedPools.push_back(grab_pool());
            currentPool = *usedPools.front();
        }

        auto setAllocateInfo = vk::DescriptorSetAllocateInfo(currentPool, 1, &layout);
        try {
            return std::move(device->allocateDescriptorSets(setAllocateInfo).front());
        } catch (vk::OutOfPoolMemoryError &) {
        } catch (vk::FragmentedPoolError &) {}

        // --- Pool Reallocation ---
        // Allocate a new pool and retry to allocate the descriptor set
        usedPools.push_back(grab_pool());
        currentPool = *usedPools.front();
        try {
            return std::move(device->allocateDescriptorSets(setAllocateInfo).front());
        } catch (vk::Error &error) {
            // Something is seriously wrong...
            std::cerr << "ERROR: Detected Vulkan error: " << error.what() << std::endl;
            abort();
        }
    }


    void DescriptorAllocator::reset_pools() {
        // Reset all used pools and add them to the free pools
        for (auto &pool : usedPools) {
            pool.reset();
            freePools.push_back(std::move(pool));
        }
        usedPools.clear();            // Clear the used pools, since we've put them all in the free pools
        currentPool = VK_NULL_HANDLE; // Reset the current pool handle back to null
    }


    vk::DescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(vk::DescriptorSetLayoutCreateInfo *info) {
        DescriptorLayoutInfo layoutInfo;
        layoutInfo.bindings.reserve(info->bindingCount);
        bool isSorted = true;
        int lastBinding = -1;

        //copy from the direct info struct into our own one
        for (uint32_t i = 0; i < info->bindingCount; i++) {
            auto binding = info->pBindings[i];
            layoutInfo.bindings.push_back(binding);

            auto newBinding = static_cast<int>(binding.binding);
            // Check that the bindings are in strict increasing order
            if (newBinding > lastBinding)
                lastBinding = newBinding;
            else
                isSorted = false;
        }
        //sort the bindings if they aren't in order
        if (!isSorted) {
            std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(), [](auto &a, auto &b) {
                return a.binding < b.binding;
            });
        }

        // Try to return cached layout; otherwise, add a new one to the cache
        auto it = layoutCache.find(layoutInfo);
        if (it != layoutCache.end()) {
            return **(*it).second;
        } else {
            layoutCache[layoutInfo] = std::make_unique<vk::raii::DescriptorSetLayout>(device->createDescriptorSetLayout(*info));
            return **layoutCache[layoutInfo];
        }
    }


    bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo &other) const {
        if (other.bindings.size() != bindings.size()) {
            return false;
        } else { // Ensure each binding is the same (will match if sorted)
            for (int i = 0; i < bindings.size(); i++) {
                auto otherBinding = other.bindings[i];
                auto binding = otherBinding;
                if (binding.binding != otherBinding.binding)
                    return false;
                if (binding.descriptorType != otherBinding.descriptorType)
                    return false;
                if (binding.descriptorCount != otherBinding.descriptorCount)
                    return false;
                if (binding.stageFlags != otherBinding.stageFlags)
                    return false;
            }
        }
        return true;
    }


    size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
        auto result = std::hash<size_t>()(bindings.size());
        for (const VkDescriptorSetLayoutBinding &binding : bindings) {
            // Pack the binding data into a single int64. Not fully correct but it's ok
            // clang-format off
        auto hash = binding.binding
            | binding.descriptorType << 8
            | binding.descriptorCount << 16
            | binding.stageFlags << 24;
            // clang-format on

            // Shuffle the packed binding data and xor it with the main hash
            result ^= std::hash<size_t>()(hash);
        }
        return result;
    }

    vkutil::DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator){
        DescriptorBuilder builder;
        builder.cache = layoutCache;
        builder.allocator = allocator;
        return builder;
    }



//    template<typename T>
//    DescriptorBuilder &DescriptorBuilder::bind(uint32_t binding, const std::vector<T> &infos, vk::DescriptorType type, vk::ShaderStageFlags stageFlags)

    std::unique_ptr<Descriptor> DescriptorBuilder::build() {
        // --- Build Layout ---
        auto layoutInfo = vk::DescriptorSetLayoutCreateInfo({}, (uint32_t) bindings.size(), bindings.data());
        auto layout = cache->create_descriptor_layout(&layoutInfo);

        // --- Allocate Descriptor ---
        auto set = allocator->allocate(layout);

        // --- Write Descriptor ---
        for (auto &write : writes) write.setDstSet(*set);

        allocator->device->updateDescriptorSets(writes, {});
        return std::make_unique<vkutil::Descriptor>(std::move(set), layout);
    }
}
