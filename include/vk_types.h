#pragma once

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

struct AllocatedBuffer {
    VkBuffer buffer; // Handle to a GPU-side Vulkan buffer
    VmaAllocation allocation;
};


struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
};
