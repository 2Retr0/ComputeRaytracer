#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct AllocatedBuffer {
    vk::Buffer buffer; // Handle to a GPU-side Vulkan buffer
    vma::Allocation allocation;
};


struct AllocatedImage {
    vk::Image image;
    vma::Allocation allocation;
};
