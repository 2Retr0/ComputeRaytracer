#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

class PipelineBuilder {
public:
    // This is a basic set of required Vulkan structs for pipeline creation, there are more, but for now these are the
    // ones we will need to fill for now.
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    vk::Viewport viewport;
    vk::Rect2D scissor;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    vk::PipelineMultisampleStateCreateInfo multisampling;
    vk::PipelineLayout pipelineLayout;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;

    vk::raii::Pipeline build_graphics_pipeline(const vk::raii::Device &device, const vk::raii::RenderPass &renderpass);
    vk::raii::Pipeline build_compute_pipeline(const vk::raii::Device &device);
};