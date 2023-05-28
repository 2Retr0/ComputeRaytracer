#pragma once

#include <vulkan/vulkan.h>
#include <deque>
#include <functional>
#include <ranges>

#include "vk_mem_alloc.h"

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push(std::function<void()>&& function) {
        deletors.push_back(function);
    }

    void flush() {
        // Reverse iterate the deletion queue to execute all the functions
        for (auto & deletor : std::ranges::reverse_view(deletors))
            deletor();
        deletors.clear();
    }
};


struct AllocatedBuffer {
    VkBuffer buffer; // Handle to a GPU-side Vulkan buffer
    VmaAllocation allocation;
};


struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
};
