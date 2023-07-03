#pragma once

#include "vk_types.h"

/** Abstractions over the initialization of Vulkan structures. */
namespace vkinit {
    VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

    VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

    VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderpass, VkExtent2D extent);

    VkSubmitInfo submit_info(VkCommandBuffer* commandBuffer);

    VkPresentInfoKHR present_info();

    VkRenderPassBeginInfo renderpass_begin_info(VkRenderPass renderpass, VkExtent2D windowExtent, VkFramebuffer framebuffer);

    /** Configuration for a single shader stage for the pipeline--built from a shader stage and a shader module. */
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

    /** Configuration for vertex buffers and vertex formats. This is equivalent to the VAO configuration on OpenGL. */
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

    /**
     * Configuration for what kind of topology will be drawn. This is where you set it to draw triangles, lines, points, or
     * others like triangle-list.
     */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

    /**
     * Configuration for the fixed-function rasterization. This is where we enable or disable backface culling, and set
     * line width or wireframe drawing.
     */
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

    /** Configuration for MSAA within the pipeline. */
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

    /** Configuration for how the pipeline blends into a given attachment. */
    VkPipelineColorBlendAttachmentState color_blend_attachment_state();

    /**
     * Configuration for information about shader inputs of a given pipeline. We would configure our push-constraints
     * and descriptor sets here.
     */
    VkPipelineLayoutCreateInfo pipeline_layout_create_info();

    VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

    VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

    VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

    VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

    /** Configuration for information about how to use depth-testing in a render pipeline. */
    VkPipelineDepthStencilStateCreateInfo
    depth_stencil_create_info(bool enableDepthTest, bool enableDepthWrite, VkCompareOp compareOperation);

    /** Configuration for a descriptor itself. */
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

    VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo , uint32_t binding);
} // namespace vkinit
