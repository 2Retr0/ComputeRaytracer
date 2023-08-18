#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct AllocatedBuffer {
    vk::Buffer buffer; // Handle to a GPU-side Vulkan buffer
    vma::Allocation allocation;

    AllocatedBuffer() = default;
    explicit AllocatedBuffer(std::pair<vk::Buffer, vma::Allocation> buffer) : buffer(buffer.first), allocation(buffer.second) {}
};


struct AllocatedImage {
    vk::Image image;
    vma::Allocation allocation;

    AllocatedBuffer() = default;
    explicit AllocatedImage(std::pair<vk::Image, vma::Allocation> image) : image(image.first), allocation(image.second) {}
};
