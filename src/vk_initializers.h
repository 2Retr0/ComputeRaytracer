#pragma once

#include <vk_types.h>

// Includes abstractions over the initialization of Vulkan structures.
namespace vkinit {
    VkCommandPoolCreateInfo command_pool_create_info(
            uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

    VkCommandBufferAllocateInfo command_buffer_allocate_info(
            VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

VkCommandPoolCreateInfo vkinit::command_pool_create_info(
        uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo commandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .queueFamilyIndex = queueFamilyIndex
    };

    return commandPoolInfo;
}


VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(
        VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
    // Primary command buffers have commands sent into a `VkQueue` to do work.
    // Secondary command buffers level are mostly used in advanced multithreading scenarios with 'subcommands'.
    VkCommandBufferAllocateInfo commandAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = pool,
            .level = level,
            .commandBufferCount = count,
    };

    return commandAllocateInfo;
}

