#include "vk_initializers.h"

vk::CommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags /*= 0*/) {
    auto info = vk::CommandPoolCreateInfo();
    info.flags = flags;
    info.queueFamilyIndex = queueFamilyIndex;

    return info;
}

vk::CommandBufferAllocateInfo vkinit::command_buffer_allocate_info(vk::CommandPool pool, uint32_t count /*= 1*/, vk::CommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/) {
    // Primary command buffers have commands sent into a `vk::Queue` to do work.
    // Secondary command buffer level is mostly used in advanced multithreading scenarios with 'subcommands'.
    auto info = vk::CommandBufferAllocateInfo();
    info.commandPool = pool;
    info.level = level;
    info.commandBufferCount = count;

    return info;
}


vk::CommandBufferBeginInfo vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlags flags /**= {}*/) {
    auto info = vk::CommandBufferBeginInfo();
    info.flags = flags;
    info.pInheritanceInfo = nullptr;

    return info;
}


vk::FramebufferCreateInfo vkinit::framebuffer_create_info(vk::RenderPass renderpass, vk::Extent2D extent) {
    auto info = vk::FramebufferCreateInfo();
    info.renderPass = renderpass;
    info.attachmentCount = 1;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1;

    return info;
}


vk::SubmitInfo vkinit::submit_info(const vk::CommandBuffer *commandBuffer) {
    auto info = vk::SubmitInfo();
    info.waitSemaphoreCount = 0;
    info.commandBufferCount = 1;
    info.pCommandBuffers = commandBuffer;
    info.signalSemaphoreCount = 0;

    return info;
}


vk::PresentInfoKHR vkinit::present_info() {
    return {};
}


vk::RenderPassBeginInfo vkinit::renderpass_begin_info(vk::RenderPass renderpass, vk::Extent2D windowExtent, vk::Framebuffer framebuffer) {
    auto info = vk::RenderPassBeginInfo();
    info.renderPass = renderpass;
    info.framebuffer = framebuffer;
    info.clearValueCount = 1;
    info.pClearValues = nullptr;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = windowExtent;

    return info;
}


vk::PipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule) {
    auto info = vk::PipelineShaderStageCreateInfo();
    info.stage = stage;         // Shader stage
    info.module = shaderModule; // Module containing the code for this shader stage
    info.pName = "main";        // The entry point of the shader

    return info;
}


vk::PipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info() {
    return {}; // No vertex bindings or attributes
}


vk::PipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(vk::PrimitiveTopology topology) {
    // --- Example Topologies ---
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: Normal triangle drawing
    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST:    Points
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST:     Line-list
    auto info = vk::PipelineInputAssemblyStateCreateInfo();
    info.topology = topology;
    info.primitiveRestartEnable = VK_FALSE; // We are not going to use primitive restart for this tutorial.

    return info;
}


vk::PipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(vk::PolygonMode polygonMode) {
    // If `rasterizerDiscardEnable` is enabled, primitives (triangles in our case) are discarded before even
    // making it to the rasterization stage, which means the triangles would never get drawn to the screen.
    // You might enable this, for example, if you’re only interested in the side effects of the vertex
    // processing stages, such as writing to a buffer which you later read from.
    auto info = vk::PipelineRasterizationStateCreateInfo();
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;     // Discards all primitives before the rasterization stage if enabled.
    info.polygonMode = info.polygonMode;         // Input for toggling between wireframe and solid drawing.
    info.cullMode = vk::CullModeFlagBits::eNone; // No backface culling
    info.frontFace = vk::FrontFace::eClockwise;
    info.depthBiasEnable = VK_FALSE;             // No depth bias
    info.depthBiasConstantFactor = 0.0f;
    info.depthBiasClamp = 0.0f;
    info.depthBiasSlopeFactor = 0.0f;
    info.lineWidth = 1.0f;

    return info;
}


vk::PipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info() {
    // We won't use MSAA for this tutorial.
    auto info = vk::PipelineMultisampleStateCreateInfo();
    info.sampleShadingEnable = VK_FALSE;
    info.rasterizationSamples = vk::SampleCountFlagBits::e1; // 1 sample as we won't be doing MSAA
    info.minSampleShading = 1.0f;
    info.pSampleMask = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable = VK_FALSE;

    return info;
}


vk::PipelineColorBlendAttachmentState vkinit::color_blend_attachment_state() {
    // We are rendering to only 1 attachment, so we will just need one of them, and defaulted to “not blend” and just
    // override. In here it’s possible to make objects that will blend with the image.
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState();
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA;

    return colorBlendAttachment;
}


vk::PipelineLayoutCreateInfo vkinit::pipeline_layout_create_info() {
    return {};
}


vk::FenceCreateInfo vkinit::fence_create_info(vk::FenceCreateFlags flags /*= 0*/) {
    return {flags};
}


vk::SemaphoreCreateInfo vkinit::semaphore_create_info(vk::SemaphoreCreateFlags flags /*= 0*/) {
    return {flags};
}


vk::ImageCreateInfo vkinit::image_create_info(vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent) {
    // Tiling describes how the data for the texture is arranged in the GPU. For improved performance, GPUs do not
    // store images as 2d arrays of pixels, but instead use complex custom formats, unique to the GPU brand and even
    // models.
    // --- Example Tilings ---
    // VK_IMAGE_TILING_OPTIMAL: Let the driver decide how the GPU arranges the memory of the image.
    //                          This prevents reading from or writing to the CPU without changing the tiling first.
    // VK_IMAGE_TILING_LINEAR: Store image as 2D array of pixels (very slow!)
    auto info = vk::ImageCreateInfo();
    info.imageType = vk::ImageType::e2D;        // How many dimensions the image has
    info.format = format;                       // What the data of the texture is (e.g., single float (for depth) or color)
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;                       // For layered textures (e.g., cubemaps which have 6 layers)
    info.samples = vk::SampleCountFlagBits::e1; // MSAA samples
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = usageFlags;                    // Controls how the GPU handles the image memory (must set properly!)

    return info;
}


vk::ImageViewCreateInfo vkinit::imageview_create_info(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags) {
    // In vulkan you can’t use `vk::Image` directly, the `vk::Image` have to go through a `vk::ImageView`, which contains some
    // information about how to treat the image. We build an image-view for the depth image to use for rendering
    auto info = vk::ImageViewCreateInfo();
    // The image has to point to the image this imageview is being created from. As `imageView`s 'wrap' an image, you
    // need to point to the original one. The format has to match the format in the image this view was created from.
    info.image = image;
    info.viewType = vk::ImageViewType::e2D; // Or `VK_IMAGE_VIEW_TYPE_CUBE`, etc.
    info.format = format;

    // subresourceRange holds the information about where the image points to. This is used for layered images, where
    // you might have multiple layers in one image, and want to create an imageview that points to a specific layer.
    info.subresourceRange.aspectMask = aspectFlags; // Controls how the GPU handles the image memory (must set properly!)
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    return info;
}


vk::PipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(bool enableDepthTest, bool enableDepthWrite, vk::CompareOp compareOperation) {
    // --- Example Depth Compare Operations ---
    // VK_COMPARE_OP_ALWAYS: Don't test depth at all.
    // VK_COMPARE_OP_LESS: Only draw if z < whatever is on the depth buffer.
    // VK_COMPARE_OP_EQUAL: Only draw if the z = whatever is on the depth buffer.
    auto info = vk::PipelineDepthStencilStateCreateInfo();
    info.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;   // Whether to enable z-culling
    info.depthWriteEnable = enableDepthWrite ? VK_TRUE : VK_FALSE; // Whether to write depth
    info.depthCompareOp = enableDepthTest ? compareOperation : vk::CompareOp::eAlways;
    info.depthBoundsTestEnable = VK_FALSE;
    info.stencilTestEnable = VK_FALSE;                             // We aren't using stencil test

    // If the depth is outside the bounds, the pixel will be skipped
    info.minDepthBounds = 0.0f; // Optional
    info.maxDepthBounds = 1.0f; // Optional

    return info;
}


vk::DescriptorSetLayoutBinding vkinit::descriptor_set_layout_binding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags, uint32_t binding) {
    auto setBinding = vk::DescriptorSetLayoutBinding();
    setBinding.binding = binding;
    setBinding.descriptorType = type;
    setBinding.descriptorCount = 1;
    setBinding.stageFlags = stageFlags;
    setBinding.pImmutableSamplers = nullptr;

    return setBinding;
}


vk::WriteDescriptorSet vkinit::write_descriptor_buffer(vk::DescriptorType type, vk::DescriptorSet destinationSet, vk::DescriptorBufferInfo *bufferInfo, uint32_t binding) {
    auto write = vk::WriteDescriptorSet();
    write.dstSet = destinationSet;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;

    return write;
}


vk::SamplerCreateInfo vkinit::sampler_create_info(vk::Filter filters, vk::SamplerAddressMode samplerAddressMode /*= vk::SamplerAddressMode::eRepeat*/) {
    auto write = vk::SamplerCreateInfo();
    write.magFilter = filters;
    write.minFilter = filters;
    write.addressModeU = samplerAddressMode;
    write.addressModeV = samplerAddressMode;
    write.addressModeW = samplerAddressMode;

    return write;
}


vk::WriteDescriptorSet vkinit::write_descriptor_image(vk::DescriptorType type, vk::DescriptorSet dstSet, vk::DescriptorImageInfo *imageInfo, uint32_t binding) {
    auto write = vk::WriteDescriptorSet();
    write.dstSet = dstSet;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = imageInfo;

    return write;
}
