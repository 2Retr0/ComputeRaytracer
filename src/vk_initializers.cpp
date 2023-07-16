#include <vk_initializers.h>

VkCommandPoolCreateInfo vkinit::command_pool_create_info(
    uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex,
    };
}

VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(
    VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/) {
    // Primary command buffers have commands sent into a `VkQueue` to do work.
    // Secondary command buffer level is mostly used in advanced multithreading scenarios with 'subcommands'.
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool,
        .level = level,
        .commandBufferCount = count,
    };
}


VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags flags /**= 0*/) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = flags,
        .pInheritanceInfo = nullptr,
    };
}


VkFramebufferCreateInfo vkinit::framebuffer_create_info(VkRenderPass renderpass, VkExtent2D extent) {
    return {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .renderPass = renderpass,
        .attachmentCount = 1,
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };
}


VkSubmitInfo vkinit::submit_info(VkCommandBuffer *commandBuffer) {
    return {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
}


VkPresentInfoKHR vkinit::present_info() {
    return {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .swapchainCount = 0,
        .pSwapchains = nullptr,
        .pImageIndices = nullptr,
    };
}


VkRenderPassBeginInfo vkinit::renderpass_begin_info(VkRenderPass renderpass, VkExtent2D windowExtent, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderpass,
        .framebuffer = framebuffer,
        .clearValueCount = 1,
        .pClearValues = nullptr,
    };
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = windowExtent;

    return info;
}


VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(
    VkShaderStageFlagBits stage, VkShaderModule shaderModule) {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .stage = stage,         // Shader stage
        .module = shaderModule, // Module containing the code for this shader stage
        .pName = "main",        // The entry point of the shader
    };
}


VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info() {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        // No vertex bindings or attributes
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };
}


VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(VkPrimitiveTopology topology) {
    // --- Example Topologies ---
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: Normal triangle drawing
    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST:    Points
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST:     Line-list
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = topology,
        .primitiveRestartEnable = VK_FALSE, // We are not going to use primitive restart for this tutorial.
    };
}


VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(VkPolygonMode polygonMode) {
    // If `rasterizerDiscardEnable` is enabled, primitives (triangles in our case) are discarded before even
    // making it to the rasterization stage, which means the triangles would never get drawn to the screen.
    // You might enable this, for example, if you’re only interested in the side effects of the vertex
    // processing stages, such as writing to a buffer which you later read from.
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE, // Discards all primitives before the rasterization stage if enabled.
        .polygonMode = polygonMode,          // Input for toggling between wireframe and solid drawing.
        .cullMode = VK_CULL_MODE_NONE,       // No backface culling
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE, // No depth bias
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };
}


VkPipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info() {
    // We won't use MSAA for this tutorial.
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // Multisampling = 1 sample as we won't be doing MSAA
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };
}


VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state() {
    // We are rendering to only 1 attachment, so we will just need one of them, and defaulted to “not blend” and just
    // override. In here it’s possible to make objects that will blend with the image.
    return {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
}


VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info() {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,

        // Empty defaults
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
}


VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags /*= 0*/) {
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}


VkSemaphoreCreateInfo vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags /*= 0*/) {
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}


VkImageCreateInfo vkinit::image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent) {
    // Tiling describes how the data for the texture is arranged in the GPU. For improved performance, GPUs do not
    // store images as 2d arrays of pixels, but instead use complex custom formats, unique to the GPU brand and even
    // models.
    // --- Example Tilings ---
    // VK_IMAGE_TILING_OPTIMAL: Let the driver decide how the GPU arranges the memory of the image.
    //                          This prevents reading from or writing to the CPU without changing the tiling first.
    // VK_IMAGE_TILING_LINEAR: Store image as 2D array of pixels (very slow!)
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D, // How many dimensions the image has
        .format = format,              // What the data of the texture is (e.g., single float (for depth) or color)
        .extent = extent,              // Size of the image, in pixels
        .mipLevels = 1,
        .arrayLayers = 1,                 // For layered textures (e.g., cubemaps which have 6 layers)
        .samples = VK_SAMPLE_COUNT_1_BIT, // MSAA samples
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags, // Controls how the GPU handles the image memory (must set properly!)
    };
}


VkImageViewCreateInfo vkinit::imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) {
    // In vulkan you can’t use `VkImage` directly, the `VkImage` have to go through a `VkImageView`, which contains some
    // information about how to treat the image. We build an image-view for the depth image to use for rendering

    // The image has to point to the image this imageview is being created from. As `imageView`s 'wrap' an image, you
    // need to point to the original one. The format has to match the format in the image this view was created from.
    VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, // Or `VK_IMAGE_VIEW_TYPE_CUBE`, etc.
        .format = format,
    };
    // subresourceRange holds the information about where the image points to. This is used for layered images, where
    // you might have multiple layers in one image, and want to create an imageview that points to a specific layer.
    info.subresourceRange = {
        .aspectMask = aspectFlags, // Controls how the GPU handles the image memory (must set properly!)
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    return info;
}


VkPipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(
    bool enableDepthTest, bool enableDepthWrite, VkCompareOp compareOperation) {
    // --- Example Depth Compare Operations ---
    // VK_COMPARE_OP_ALWAYS: Don't test depth at all.
    // VK_COMPARE_OP_LESS: Only draw if z < whatever is on the depth buffer.
    // VK_COMPARE_OP_EQUAL: Only draw if the z = whatever is on the depth buffer.
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE,   // Whether to enable z-culling
        .depthWriteEnable = enableDepthWrite ? VK_TRUE : VK_FALSE, // Whether to write depth
        .depthCompareOp = enableDepthTest ? compareOperation : VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE, // We aren't using stencil test

        // If the depth is outside the bounds, the pixel will be skipped
        .minDepthBounds = 0.0f, // Optional
        .maxDepthBounds = 1.0f, // Optional
    };
}


VkDescriptorSetLayoutBinding vkinit::descriptor_set_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding) {
    return {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr,
    };
}


VkWriteDescriptorSet vkinit::write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet destinationSet, VkDescriptorBufferInfo *bufferInfo, uint32_t binding) {
    return {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = destinationSet,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = bufferInfo,
    };
}


VkSamplerCreateInfo vkinit::sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/) {
    return {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = filters,
        .minFilter = filters,
        .addressModeU = samplerAddressMode,
        .addressModeV = samplerAddressMode,
        .addressModeW = samplerAddressMode,
    };
}


VkWriteDescriptorSet vkinit::write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding) {
    return {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = dstSet,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = imageInfo,
    };
}
