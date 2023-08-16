#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct AllocatedBufferUntyped {
    vk::Buffer buffer{}; // Handle to a GPU-side Vulkan buffer
    vma::Allocation allocation{};
    vk::DeviceSize size = 0;

    vk::DescriptorBufferInfo get_info(VkDeviceSize offset = 0) const;
};

template<typename T>
struct AllocatedBuffer : public AllocatedBufferUntyped {
    AllocatedBuffer() = default;

    explicit AllocatedBuffer(AllocatedBufferUntyped& other) {
        this(other.buffer, other.allocation, other.size);
    }

    AllocatedBuffer &operator=(AllocatedBufferUntyped other) {
        buffer = other.buffer;
        allocation = other.allocation;
        size = other.size;
        return *this;
    }
};

struct AllocatedImage {
    vk::Image image;
    vma::Allocation allocation;
};

inline vk::DescriptorBufferInfo AllocatedBufferUntyped::get_info(vk::DeviceSize offset /**=0*/) const {
    return {buffer, offset, size};
}
