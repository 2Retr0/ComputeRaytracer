#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "vk_types.h"

/** Abstractions over the initialization of Vulkan structures. */
namespace vkinit {
    vk::CommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags = {});

    vk::CommandBufferAllocateInfo command_buffer_allocate_info(vk::CommandPool pool, uint32_t count = 1, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    vk::CommandBufferBeginInfo command_buffer_begin_info(vk::CommandBufferUsageFlags flags = {});

    vk::FramebufferCreateInfo framebuffer_create_info(vk::RenderPass renderpass, vk::Extent2D extent);

    vk::SubmitInfo submit_info(const vk::CommandBuffer *commandBuffer);

    vk::PresentInfoKHR present_info();

    vk::RenderPassBeginInfo renderpass_begin_info(vk::RenderPass renderpass, vk::Extent2D windowExtent, vk::Framebuffer framebuffer);

    /** Configuration for a single shader stage for the pipeline--built from a shader stage and a shader module. */
    vk::PipelineShaderStageCreateInfo pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule);

    /** Configuration for vertex buffers and vertex formats. This is equivalent to the VAO configuration on OpenGL. */
    vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info();

    /**
     * Configuration for what kind of topology will be drawn. This is where you set it to draw triangles, lines, points, or
     * others like triangle-list.
     */
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info(vk::PrimitiveTopology topology);

    /**
     * Configuration for the fixed-function rasterization. This is where we enable or disable backface culling, and set
     * line width or wireframe drawing.
     */
    vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info(vk::PolygonMode polygonMode);

    /** Configuration for MSAA within the pipeline. */
    vk::PipelineMultisampleStateCreateInfo multisampling_state_create_info();

    /** Configuration for how the pipeline blends into a given attachment. */
    vk::PipelineColorBlendAttachmentState color_blend_attachment_state();

    /**
     * Configuration for information about shader inputs of a given pipeline. We would configure our push-constraints
     * and descriptor sets here.
     */
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info();

    vk::FenceCreateInfo fence_create_info(vk::FenceCreateFlags flags = {});

    vk::SemaphoreCreateInfo semaphore_create_info(vk::SemaphoreCreateFlags flags = {});

    vk::ImageCreateInfo image_create_info(vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent);

    vk::ImageViewCreateInfo imageview_create_info(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);

    /** Configuration for information about how to use depth-testing in a render pipeline. */
    vk::PipelineDepthStencilStateCreateInfo
    depth_stencil_create_info(bool enableDepthTest, bool enableDepthWrite, vk::CompareOp compareOperation);

    /** Configuration for a descriptor itself. */
    vk::DescriptorSetLayoutBinding descriptor_set_layout_binding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags, uint32_t binding);

    vk::WriteDescriptorSet write_descriptor_buffer(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorBufferInfo* bufferInfo , uint32_t binding);

    vk::SamplerCreateInfo sampler_create_info(vk::Filter filters, vk::SamplerAddressMode samplerAddressMode = vk::SamplerAddressMode::eRepeat);

    vk::WriteDescriptorSet write_descriptor_image(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorImageInfo* imageInfo, uint32_t binding);

    vk::BufferMemoryBarrier buffer_memory_barrier(vk::Buffer buffer, uint32_t queue);

    vk::ImageMemoryBarrier image_memory_barrier(vk::Image image, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask);
} // namespace vkinit
