#include "vk_material.h"

vk::raii::Pipeline PipelineBuilder::build_graphics_pipeline(const vk::raii::Device &device, const vk::raii::RenderPass &renderpass) {
    // Let’s begin by connecting the viewport and scissor into `ViewportState`, and setting the
    // `ColorBlenderStateCreateInfo`.
    // Make viewport state from our stored viewport and scissor. At the moment, we won't support multiple viewports or
    // scissors.
    auto viewportState = vk::PipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor);

    // Setup dummy color blending. We aren't using transparent objects yet, so the blending is just set to "no blend",
    // but we do write to the color attachment.
    auto colorBlending = vk::PipelineColorBlendStateCreateInfo({}, false, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

//    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
//    auto dynamicState = vk::PipelineDynamicStateCreateInfo({}, dynamicStates);

        // Build the actual pipeline. We will use all the info structs we have been writing into this one for creation.
    // clang-format off
    auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
        .setStageCount(shaderStages.size())
        .setPStages(shaderStages.data())
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depthStencil)
        .setPColorBlendState(&colorBlending)
        .setLayout(pipelineLayout)
        .setRenderPass(*renderpass)
        .setSubpass(0)
        .setBasePipelineHandle(nullptr);
//        .setPDynamicState(&dynamicState);
    // clang-format on

    // It's easy to error when creating the graphics pipeline, so we handle it better than just using VK_CHECK.
    return device.createGraphicsPipeline(nullptr, pipelineInfo);
}


vk::raii::Pipeline PipelineBuilder::build_compute_pipeline(const vk::raii::Device &device) {
    // clang-format off
    auto pipelineInfo = vk::ComputePipelineCreateInfo()
        .setStage(shaderStages.front())
        .setLayout(pipelineLayout);
    // clang-format on

    return device.createComputePipeline(nullptr, pipelineInfo);
}
