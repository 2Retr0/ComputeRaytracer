#include "vk_descriptors.h"

#include <iostream>

vk::raii::DescriptorPool createPool(const vk::raii::Device &device, const DescriptorAllocator::PoolSizes &poolSizes, int count, vk::DescriptorPoolCreateFlagBits flags) {
    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(poolSizes.sizes.size());

    for (auto &[type, ratio] : poolSizes.sizes) {
        sizes.emplace_back(type, static_cast<uint32_t>(ratio * static_cast<float>(count)));
    }
    auto poolInfo = vk::DescriptorPoolCreateInfo(flags, count, static_cast<uint32_t>(sizes.size()), sizes.data());

    return {device, poolInfo};
}

vk::raii::DescriptorPool DescriptorAllocator::grab_pool() {
    if (!freePools.empty()) { // Reusable pools are available
        auto pool = std::move(freePools.back());
        freePools.pop_back();
        return pool;
    } else { // No pools available, so create a new one
        return createPool(device, descriptorSizes, 1000, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    }
}


vk::raii::DescriptorSet DescriptorAllocator::allocate(vk::raii::DescriptorSetLayout layout) {
    //initialize the currentPool handle if it's null
    if (!currentPool) {
        usedPools.push_back(grab_pool());
        currentPool = *usedPools.front();
    }

    auto setAllocateInfo = vk::DescriptorSetAllocateInfo(currentPool, 1, &(*layout));
    try {
        return std::move(device.allocateDescriptorSets(setAllocateInfo).front());
    } catch (vk::OutOfPoolMemoryError &ignored) {
    } catch (vk::FragmentedPoolError &ignored) {}

    // --- Pool Reallocation ---
    // Allocate a new pool and retry to allocate the descriptor set
    usedPools.push_back(grab_pool());
    currentPool = *usedPools.front();
    try {
        return std::move(device.allocateDescriptorSets(setAllocateInfo).front());
    } catch (vk::Error &error) {
        // Something is seriously wrong...
        std::cerr << "ERROR: Detected Vulkan error: " << error.what() << std::endl;
        abort();
    }
}


void DescriptorAllocator::reset_pools(){
    // Reset all used pools and add them to the free pools
    for (auto &pool : usedPools){
        pool.reset();
        freePools.push_back(std::move(pool));
    }

    //clear the used pools, since we've put them all in the free pools
    usedPools.clear();

    //reset the current pool handle back to null
    currentPool = VK_NULL_HANDLE;
}
