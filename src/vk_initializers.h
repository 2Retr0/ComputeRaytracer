#pragma once

#include <vk_types.h>

// Includes abstractions over the initialization of Vulkan structures.
namespace vkinit {
    VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

    VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

    VkPipelineColorBlendAttachmentState color_blend_attachment_state();

    VkPipelineLayoutCreateInfo pipeline_layout_create_info();
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

/**
 * Configuration for a single shader stage for the pipeline--built from a shader stage and a shader module.
 */
VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(
        VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
    VkPipelineShaderStageCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = stage,         // Shader stage
            .module = shaderModule, // Module containing the code for this shader stage
            .pName = "main",        // The entry point of the shader
    };

    return info;
}

/**
 * Configuration for vertex buffers and vertex formats. This is equivalent to the VAO configuration on opengl.
 */
VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info() {
    VkPipelineVertexInputStateCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            // No vertex bindings or attributes
            .vertexBindingDescriptionCount = 0,
            .vertexAttributeDescriptionCount = 0,
    };

    return info;
}

/**
 * Configuration for what kind of topology will be drawn. This is where you set it to draw triangles, lines, points, or
 * others like triangle-list.
 */
VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(VkPrimitiveTopology topology) {
    // --- Example Topologies ---
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: Normal triangle drawing
    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST:    Points
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST:     Line-list
    VkPipelineInputAssemblyStateCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .topology = topology,
            // We are not going to use primitive restart for this tutorial, so leave it false
            .primitiveRestartEnable = VK_FALSE,
    };

    return info;
}

/**
 * Configuration for the fixed-function rasterization. This is where we enable or disable backface culling, and set
 * line width or wireframe drawing.
 */
VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(VkPolygonMode polygonMode) {
    // We leave `polygonMode` as an input to be able to toggle between wireframe and solid drawing.
    VkPipelineRasterizationStateCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .depthClampEnable = VK_FALSE,

            // If rasterizerDiscardEnable is enabled, primitives (triangles in our case) are discarded before even
            // making it to the rasterization stage which means the triangles would never get drawn to the screen.
            // You might enable this, for example, if you’re only interested in the side effects of the vertex
            // processing stages, such as writing to a buffer which you later read from.
            .rasterizerDiscardEnable = VK_FALSE, // Discards all primitives before the rasterization stage if enabled.

            .polygonMode = polygonMode,
            .cullMode = VK_CULL_MODE_NONE,       // No backface culling
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,         // No depth bias
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f,
    };

    return info;
}

/**
 * Configuration for MSAA within the pipeline.
 */
VkPipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info() {
    // We won't use MSAA for this tutorial.
    VkPipelineMultisampleStateCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // Multisampling = 1 sample as we won't be doing MSAA
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
    };

    return info;
}

/**
 * Configuration for how the pipeline blends into a given attachment.
 */
VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state() {
    // We are rendering to only 1 attachment, so we will just need one of them, and defaulted to “not blend” and just
    // override. In here it’s possible to make objects that will blend with the image.
    VkPipelineColorBlendAttachmentState info = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    return info;
}


/**
 * Configuration for information about shader inputs of a given pipeline. We would configure our push-constraints and
 * descriptor sets here.
 */
VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info() {
    VkPipelineLayoutCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        // Empty defaults
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };

    return info;
}