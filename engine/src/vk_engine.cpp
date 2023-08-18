#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_textures.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vkBootstrap.h>
#include <VkBootstrapDispatch.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <fstream>
#include <functional>
#include <iostream>

// We want to immediately abort when there is an error. In normal engines, this would give an error message to the
// user, or perform a dump of state.
#define VK_CHECK(x)                                                       \
    do {                                                                  \
        vk::Result error = x;                                             \
        if (error != vk::Result::eSuccess) {                              \
            std::cout << "Detected Vulkan error: " << error << std::endl; \
            abort();                                                      \
        }                                                                 \
    } while (0)

#ifdef NDEBUG
constexpr bool useValidationLayers = false;
#else
constexpr bool useValidationLayers = true;
#endif

vk::raii::Pipeline PipelineBuilder::build_pipeline(const vk::raii::Device &device, const vk::raii::RenderPass &renderpass) {
    // Let’s begin by connecting the viewport and scissor into `ViewportState`, and setting the
    // `ColorBlenderStateCreateInfo`.
    // Make viewport state from our stored viewport and scissor. At the moment, we won't support multiple viewports or
    // scissors.
    auto viewportState = vk::PipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor);

    // Setup dummy color blending. We aren't using transparent objects yet, so the blending is just set to "no blend",
    // but we do write to the color attachment.
    auto colorBlending = vk::PipelineColorBlendStateCreateInfo({}, false, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

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
    // clang-format on

    // It's easy to error when creating the graphics pipeline, so we handle it better than just using VK_CHECK.
    return {device, nullptr, pipelineInfo};
}


void VulkanEngine::init() {
    try {
        // We initialize SDL and create a window with it. `SDL_INIT_VIDEO` tells SDL that we want the main windowing
        // functionality (includes basic input events like keys or mouse).
        SDL_Init(SDL_INIT_VIDEO);

        auto windowFlags = (SDL_WindowFlags) (SDL_WINDOW_VULKAN);
        auto windowTitle = std::string("VulkanTest2") + (useValidationLayers ? " (DEBUG)" : "");
        // Create a blank SDL window for our application
        window = SDL_CreateWindow(
            windowTitle.c_str(),                   // Window title
            SDL_WINDOWPOS_UNDEFINED,               // Window x position (don't care)
            SDL_WINDOWPOS_UNDEFINED,               // Window y position (don't care)
            static_cast<int>(windowExtent.width),  // Window height (px)
            static_cast<int>(windowExtent.height), // Window width  (px)
            windowFlags);

        init_vulkan();
        init_swapchain();
        init_commands();
        init_default_renderpass();
        init_framebuffers();
        init_sync_structures();
        init_descriptors(); // Some initialized descriptors are needed when creating the pipelines.
        init_pipelines();
        load_images();
        load_meshes();
        init_scene();
        init_imgui();
    } catch (vk::SystemError &error) {
        std::cout << "Detected Vulkan error: " << error.what() << std::endl;
        abort();
    } catch (std::exception &error) {
        std::cout << "Encountered std::exception: " << error.what() << std::endl;
        abort();
    } catch (...) {
        std::cout << "Encountered unknown error\n";
        abort();
    }

    // Sort the renderables array before rendering by Pipeline and Mesh, to reduce the number of binds.
    std::sort(renderables.begin(), renderables.end(), [&](const RenderObject &a, const RenderObject &b) {
        auto greaterMaterialPtr = a.material > b.material;
        auto greaterMeshPtr = a.mesh > b.mesh;

        return (greaterMaterialPtr && greaterMeshPtr) || greaterMeshPtr;
    });

    // Everything went fine!
    isInitialized = true;
}


void VulkanEngine::init_vulkan() {
    // clang-format off
    // --- Initialize Vulkan Instance ---
    vkb::InstanceBuilder builder;

    // Make the Vulkan instance, with basic debug features
    auto vkbInstance = builder.set_app_name("VulkanTest2")
       .request_validation_layers(useValidationLayers)
       .require_api_version(1, 1, 0)
       .use_default_debug_messenger()
       .build()
       .value();

    context = vk::raii::Context();
    instance = vk::raii::Instance(context, vkbInstance.instance);
    debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance, vkbInstance.debug_messenger);

    // --- Initialize Vulkan Device ---
    // Get the surface of the window we opened with SDL
    VkSurfaceKHR tempSurface;
    SDL_Vulkan_CreateSurface(window, *instance, &tempSurface);
    surface = vk::raii::SurfaceKHR(instance, tempSurface);

    // Use `vkBootstrap` to select a GPU. We want a GPU that can write to the SDL surface and supports Vulkan 1.1.
    auto selector = vkb::PhysicalDeviceSelector(vkbInstance);
    auto vkbPhysicalDevice = selector
        .set_minimum_version(1, 1)
        .set_surface(*surface)
        .select()
        .value();

    // Create the final Vulkan device from the chosen `vk::PhysicalDevice`
    auto deviceBuilder = vkb::DeviceBuilder(vkbPhysicalDevice);
    // Enable the shader draw parameters feature
    auto shaderDrawParametersFeatures = vk::PhysicalDeviceShaderDrawParametersFeatures(true);
    auto vkbDevice = deviceBuilder.add_pNext(&shaderDrawParametersFeatures).build().value();

    // Get the `vk::Device` handle used in the rest of the Vulkan application.
    chosenGPU = vk::raii::PhysicalDevice(instance, vkbPhysicalDevice.physical_device);
    device = vk::raii::Device(chosenGPU, vkbDevice.device);

    // --- Grabbing Queues ---
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // --- Initialize Memory Allocator ---
    allocator = vma::createAllocatorUnique(vma::AllocatorCreateInfo()
        .setPhysicalDevice(*chosenGPU)
        .setDevice(*device)
        .setInstance(*instance));
    // clang-format on

    // Allocating multiple things into the same buffer with descriptors pointing to each part is generally a good idea
    // in Vulkan. The main complication that comes from sub-allocating data on a buffer is that you need to be very
    // mindful of alignment.
    // GPUs often cannot read from an arbitrary address, and buffer offsets have to be aligned into a certain minimum
    // size. To know what is the minimum alignment size for buffers, we need to query it from the GPU.
    gpuProperties = vkbDevice.physical_device.properties;
    std::cout << "Selected GPU has a minimum buffer alignment of "
              << gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
}


void VulkanEngine::init_swapchain() {
    auto swapchainBuilder = vkb::SwapchainBuilder(*chosenGPU, *device, *surface);

    // --- Present Modes ---
    // VK_PRESENT_MODE_IMMEDIATE_KHR:    Immediate
    // VK_PRESENT_MODE_FIFO_KHR:         Strong VSync
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR: Adaptive VSync (immediate if below target)
    // VK_PRESENT_MODE_MAILBOX_KHR:      Triple-buffering without strong VSync
    // clang-format off
    auto vkbSwapchain = swapchainBuilder
        .use_default_format_selection()
        // An easy way to limit FPS for now.
        .set_desired_present_mode(VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
        // If you need to resize the window, the swapchain will need to be rebuilt.
        .set_desired_extent(windowExtent.width, windowExtent.height)
        .build()
        .value();

    // Store swapchain and its related images
    swapchain = vk::raii::SwapchainKHR(device, vkbSwapchain.swapchain);
    swapchainImages = vkbSwapchain.get_images().value();

    auto imageViews = vkbSwapchain.get_image_views().value();
    for (const auto &imageView : imageViews)
        swapchainImageViews.emplace_back(device, imageView);
    swapchainImageFormat = vk::Format(vkbSwapchain.image_format);

    // --- Depth Image ---
    auto depthImageExtent = vk::Extent3D(windowExtent.width, windowExtent.height, 1); // Depth image size matches window

    depthFormat = vk::Format::eD32Sfloat; // Hard-coding depth format to 32-bit float (most GPUs support it).
    // The depth image will be an image with the format we selected and Depth Attachment usage flag
    auto depthImageInfo = vkinit::image_create_info(depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment, depthImageExtent);
    // For the depth image, we want to allocate it from GPU local memory
    auto depthImageAllocationInfo = vma::AllocationCreateInfo()
        .setUsage(vma::MemoryUsage::eGpuOnly)                        // Ensures image is allocated on VRAM.
        .setRequiredFlags(vk::MemoryPropertyFlagBits::eDeviceLocal); // Forces VMA to allocate on VRAM.
    // clang-format on

    // Allocate and create the image
    depthImage = static_cast<AllocatedImage>(allocator->createImage(depthImageInfo, depthImageAllocationInfo));

    // Build an image-view for the depth image to use for rendering
    auto depthViewInfo = vkinit::imageview_create_info(depthFormat, depthImage.image, vk::ImageAspectFlagBits::eDepth);
    depthImageView = vk::raii::ImageView(device, depthViewInfo);

    // --- Cleanup ---
    mainDeletionQueue.push([this]() {
        allocator->destroyImage(depthImage.image, depthImage.allocation);
    });
}


void VulkanEngine::init_commands() {
    // Create a command pool for commands submitted to the graphics queue.
    // We want the pool to allow the resetting of individual command buffers.
    auto commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    auto uploadCommandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily);

    for (auto &frame : frames) {
        frame.commandPool = vk::raii::CommandPool(device, commandPoolInfo);

        // Allocate the other default command buffer that we will use for rendering.
        auto commandAllocateInfo = vkinit::command_buffer_allocate_info(*frame.commandPool, 1);
        auto commandBuffers = vk::raii::CommandBuffers(device, commandAllocateInfo);
        frame.mainCommandBuffer = vk::raii::CommandBuffer(std::move(commandBuffers[0]));
    }

    // Create pool for upload context
    uploadContext.commandPool = vk::raii::CommandPool(device, uploadCommandPoolInfo);

    // Allocate the default command buffer that we will use for the instant commands
    auto instantCommandAllocateInfo = vkinit::command_buffer_allocate_info(*uploadContext.commandPool, 1);
    auto instantCommandBuffers = vk::raii::CommandBuffers(device, instantCommandAllocateInfo);
    uploadContext.commandBuffer = vk::raii::CommandBuffer(std::move(instantCommandBuffers.front()));
}


void VulkanEngine::init_default_renderpass() {
    // clang-format off
    // Color Attachments are the description of the image we will be writing into with rendering commands.
    auto colorAttachment = vk::AttachmentDescription()
        .setFormat(swapchainImageFormat)                     // Attachment will have the format needed by the swapchain
        .setSamples(vk::SampleCountFlagBits::e1)             // Only use 1 sample as we won't be doing MSAA
        .setLoadOp(vk::AttachmentLoadOp::eClear)             // Clear when this attachment is loaded
        .setStoreOp(vk::AttachmentStoreOp::eStore)           // Keep the attachment stored when the renderpass ends
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)   // We don't care about stencils
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare) //           """
        .setInitialLayout(vk::ImageLayout::eUndefined)       // We don't know (or care) about the starting layout of the attachment
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);    // After the renderpass ends, the image has to be on a layout ready for display

    auto colorAttachmentReference = vk::AttachmentReference()
        .setAttachment(0) // Attachment number will index into the `pAttachments` array in the parent renderpass
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto depthAttachment = vk::AttachmentDescription()
        .setFlags({})
        .setFormat(depthFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    auto depthAttachmentReference = vk::AttachmentReference()
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // We are going to create 1 subpass (the minimum amount possible)
    auto subpass = vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colorAttachmentReference)        // Hook the color attachment into the subpass
        .setPDepthStencilAttachment(&depthAttachmentReference); // Hook the depth attachment into the subpass

    auto colorDependency = vk::SubpassDependency()
        .setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setSrcAccessMask({})
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    // We need to make sure that one frame cannot override a depth buffer while a previous frame is still rendering to
    // it. This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before previous
    // renderpasses have finished using it.
    auto depthDependency = vk::SubpassDependency()
        .setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
        .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
        .setSrcAccessMask({})
        .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    // The image life will go something like this:
    //   * <undefined> -> renderpass begins -> sub-pass 0 begins (transition to attachment optimal)
    //                 -> sub-pass 0 renders -> sub-pass 0 ends
    //                 -> renderpass ends (transition to present source)
    vk::AttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
    vk::SubpassDependency dependencies[2] = {colorDependency, depthDependency};
    // The Vulkan driver will perform the layout transitions for us when using the renderpass. If we weren't using a
    // renderpass (drawing from compute shaders), we would need to do the same transitions explicitly.
    auto renderpassInfo = vk::RenderPassCreateInfo()
        .setAttachmentCount(2)
        .setPAttachments(&attachments[0])
        .setSubpassCount(1)
        .setPSubpasses(&subpass)
        .setDependencyCount(2)
        .setPDependencies(&dependencies[0]);
    // clang-format on
    renderpass = vk::raii::RenderPass(device, renderpassInfo);
}


void VulkanEngine::init_framebuffers() {
    const auto swapchainImageCount = swapchainImages.size();
    // Create the framebuffers for the swapchain images. This will connect the renderpass to the images for rendering.
    framebuffers = std::vector<vk::raii::Framebuffer>();

    // Create framebuffers for each of the swapchain image views. We connect the depth image view when creating each of
    // the framebuffers. We do not need to change the depth image between frames and can just clear and reuse the same
    // depth image for every.
    auto framebufferInfo = vkinit::framebuffer_create_info(*renderpass, windowExtent);
    for (int i = 0; i < swapchainImageCount; i++) {
        vk::ImageView attachments[2] = {*swapchainImageViews[i], *depthImageView};
        framebufferInfo.setAttachmentCount(2);
        framebufferInfo.setPAttachments(attachments);

        framebuffers.emplace_back(device, framebufferInfo);
    }
}


void VulkanEngine::init_sync_structures() {
    // We want to create the fence with the `CREATE_SIGNALED` flag, so we can wait on it before using it on a
    // GPU command (for the first frame)
    auto fenceCreateInfo = vkinit::fence_create_info(vk::FenceCreateFlagBits::eSignaled);
    auto semaphoreCreateInfo = vkinit::semaphore_create_info();
    auto uploadFenceCreateInfo = vkinit::fence_create_info();

    uploadContext.uploadFence = vk::raii::Fence(device, uploadFenceCreateInfo);

    for (auto &frame : frames) {
        // --- Create Fence ---
        frame.renderFence = vk::raii::Fence(device, fenceCreateInfo);

        // --- Create Semaphores ---
        frame.presentSemaphore = vk::raii::Semaphore(device, semaphoreCreateInfo);
        frame.renderSemaphore = vk::raii::Semaphore(device, semaphoreCreateInfo);
    }
}


vk::raii::ShaderModule VulkanEngine::load_shader_module(const char *path) const {
    // Open the file with cursor at the end. We use `std::ios::binary` to open the stream in binary and `std::ios::ate`
    // to open the stream at the end of the file.
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Could not load shader module");

    // Find what the size of the file is by looking up the location of the cursor.
    // Because the cursor is at the end, it gives the size directly in bytes.
    auto fileSize = static_cast<std::streamsize>(file.tellg());

    // SPIR-V expects the buffer to be a uint32_t, so make sure to reserve an int vector big enough for the entire file.
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);                               // Put file cursor at the beginning
    file.read((char *) buffer.data(), fileSize); // Load the entire file into the buffer
    file.close();                                // Close file after loading

    // Create a new shader module using the loaded buffer.
    auto createInfo = vk::ShaderModuleCreateInfo()
        .setCodeSize(buffer.size() * sizeof(uint32_t)) // `codeSize` has to be in bytes
        .setPCode(buffer.data());

    // Check if the shader module creation goes well. It's very common to have errors that will fail creation, so we
    // will not use `VK_CHECK` here.
    return {device, createInfo};
}


void VulkanEngine::init_pipelines() {
    // --- Shader Modules ---
    std::unordered_map<std::string, std::unique_ptr<vk::raii::ShaderModule>> shaderModules;
    std::string shaderBaseDirectory = "../shaders/";
    std::string shaderNames[] = {
        "default_lit.frag",
        "textured_lit.frag",
        "tri_mesh.vert",
    };

    for (const auto &shaderName : shaderNames) {
        auto filepath = shaderBaseDirectory + shaderName + ".spv";
        try {
            shaderModules[shaderName] = std::make_unique<vk::raii::ShaderModule>(load_shader_module(filepath.c_str()));
        } catch (...) {
            std::cout << "ERROR: Could not load shader module " << shaderName << std::endl;
            continue;
        }
        std::cout << "Successfully loaded shader module " + shaderName << std::endl;
    }


    // --- Pipeline Setup ---
    PipelineBuilder pipelineBuilder;
    // Vertex input controls how to read vertices from vertex buffers. We aren't using it yet.
    pipelineBuilder.vertexInputInfo = vkinit::vertex_input_state_create_info();

    // Input assembly is the configuration for drawing triangle lists, strips, or individual points. We are just going
    // to draw a triangle list.
    pipelineBuilder.inputAssembly = vkinit::input_assembly_create_info(vk::PrimitiveTopology::eTriangleList);

    // Build viewport and scissor from the swapchain extents.
    pipelineBuilder.viewport.x = 0.0f;
    pipelineBuilder.viewport.y = 0.0f;
    pipelineBuilder.viewport.width = static_cast<float>(windowExtent.width);
    pipelineBuilder.viewport.height = static_cast<float>(windowExtent.height);
    pipelineBuilder.viewport.minDepth = 0.0f;
    pipelineBuilder.viewport.maxDepth = 1.0f;

    pipelineBuilder.scissor.offset = vk::Offset2D(0, 0);
    pipelineBuilder.scissor.extent = windowExtent;

    // Configure the rasterizer to draw filled triangles.
    pipelineBuilder.rasterizer = vkinit::rasterization_state_create_info(vk::PolygonMode::eFill);
    // We don't use multisampling, so just run the default one.
    pipelineBuilder.multisampling = vkinit::multisampling_state_create_info();
    // We use a single blend attachment with no blending and writing to RGBA.
    pipelineBuilder.colorBlendAttachment = vkinit::color_blend_attachment_state();
    // Default depth testing
    pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, vk::CompareOp::eLessOrEqual);

    auto vertexDescription = Vertex::get_vertex_description();
    // Connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

    pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();


    // --- Colored Mesh Pipeline Layout ---
    // Build the pipeline layout that controls the inputs/outputs of the shader.
    // We are not using descriptor sets or other systems yet, so no need to use anything other than empty default.
    auto meshPipelineLayoutInfo = vkinit::pipeline_layout_create_info();

    // Setup push constants
    auto pushConstant = vk::PushConstantRange()
        .setStageFlags(vk::ShaderStageFlagBits::eVertex) // Push constant range is accessible only in the vertex shader
        .setOffset(0)                                    // Push constant range starts at the beginning
        .setSize(sizeof(MeshPushConstants));             // Push constant range has size of `MeshPushConstants` struct

    meshPipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    meshPipelineLayoutInfo.pushConstantRangeCount = 1;

    // Hook the global set layout--we need to let the pipeline know what descriptors will be bound to it.
    vk::DescriptorSetLayout coloredSetLayouts[] = { *globalSetLayout, *objectSetLayout };
    meshPipelineLayoutInfo.setLayoutCount = 2;
    meshPipelineLayoutInfo.pSetLayouts = coloredSetLayouts;

    auto meshPipelineLayout = vk::raii::PipelineLayout(device, meshPipelineLayoutInfo);

    // --- Colored Mesh Pipeline ---
    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, **shaderModules["tri_mesh.vert"]));
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, **shaderModules["default_lit.frag"]));

    pipelineBuilder.pipelineLayout = *meshPipelineLayout; // Use the mesh pipeline layout w/ push constants we created.

    auto coloredMeshPipeline = pipelineBuilder.build_pipeline(device, renderpass);
    // Now our mesh pipeline has the space for the push constants, so we can now execute the command to use them.
    create_material(std::move(coloredMeshPipeline), std::move(meshPipelineLayout), "defaultmesh");


    // --- Textured Mesh Pipeline Layout ---
    auto texturedPipelineLayoutInfo = meshPipelineLayoutInfo;
    vk::DescriptorSetLayout texturedSetLayouts[] = {*globalSetLayout, *objectSetLayout, *singleTextureSetLayout};

    texturedPipelineLayoutInfo.setLayoutCount = 3;
    texturedPipelineLayoutInfo.pSetLayouts = texturedSetLayouts;

    auto texturedPipelineLayout = vk::raii::PipelineLayout(device, texturedPipelineLayoutInfo);
    auto texturedPipelineLayout2 = vk::raii::PipelineLayout(device, texturedPipelineLayoutInfo);

    // --- Textured Mesh Pipeline ---
    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, **shaderModules["tri_mesh.vert"]));
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, **shaderModules["textured_lit.frag"]));

    pipelineBuilder.pipelineLayout = *texturedPipelineLayout; // Connect the new pipeline layout to the pipeline builder

    auto texturedMeshPipeline = pipelineBuilder.build_pipeline(device, renderpass);
    auto texturedMeshPipeline2 = pipelineBuilder.build_pipeline(device, renderpass);
    create_material(std::move(texturedMeshPipeline), std::move(texturedPipelineLayout), "texturedmesh");
    create_material(std::move(texturedMeshPipeline2), std::move(texturedPipelineLayout2), "texturedmesh2");
}


void VulkanEngine::load_images() {
    auto lostEmpireImage = vkutil::load_image_from_asset(*this, "../assets/lost_empire-RGBA.tx");
    auto imageviewInfo = vkinit::imageview_create_info(vk::Format::eR8G8B8A8Unorm, lostEmpireImage.image, vk::ImageAspectFlagBits::eColor);
    auto lostEmpire = Texture(lostEmpireImage, vk::raii::ImageView(device, imageviewInfo));
    loadedTextures["empire_diffuse"] = Texture(std::move(lostEmpire));

    auto fumoImage = vkutil::load_image_from_asset(*this, "../assets/cirno_low_u1_v1.tx");
    auto fumoImageviewInfo = vkinit::imageview_create_info(vk::Format::eR8G8B8A8Unorm, fumoImage.image, vk::ImageAspectFlagBits::eColor);
    auto fumo = Texture(fumoImage, vk::raii::ImageView(device, fumoImageviewInfo));
    loadedTextures["fumo_diffuse"] = Texture(std::move(fumo));
}


void VulkanEngine::load_meshes() {
    Mesh triangleMesh, monkeyMesh, fumoMesh, lostEmpireMesh;
    triangleMesh.vertices.resize(3);

    triangleMesh.vertices[0].position = {1.f, 1.f, 0.f};
    triangleMesh.vertices[1].position = {-1.f, 1.f, 0.f};
    triangleMesh.vertices[2].position = {0.f, -1.f, 0.f};

    triangleMesh.vertices[0].color = {0.f, 1.f, 0.f};
    triangleMesh.vertices[1].color = {0.f, 1.f, 0.f};
    triangleMesh.vertices[2].color = {0.f, 1.f, 0.f};

    monkeyMesh.load_mesh_from_asset("../assets/monkey_smooth.mesh");
    fumoMesh.load_mesh_from_asset("../assets/cirno_low.mesh");
    lostEmpireMesh.load_mesh_from_asset("../assets/lost_empire.mesh");

    // We need to make sure both meshes are sent to the GPU. We don't care about vertex normals.
    upload_mesh(triangleMesh);
    upload_mesh(monkeyMesh);
    upload_mesh(fumoMesh);
    upload_mesh(lostEmpireMesh);

    // Note: that we are copying them. Eventually we will delete the hardcoded `monkey` and `triangle` meshes, so it's
    //       no problem now.
    meshes["triangle"] = Mesh(std::move(triangleMesh));
    meshes["monkey"] = Mesh(std::move(monkeyMesh));
    meshes["fumo"] = Mesh(std::move(fumoMesh));
    meshes["empire"] = Mesh(std::move(lostEmpireMesh));
}


void VulkanEngine::upload_mesh(Mesh &mesh) {
    const auto bufferSize = mesh.vertices.size() * sizeof(Vertex);

    // --- CPU-side Buffer Allocation ---
    // Create staging buffer to hold the mesh data before uploading to GPU buffer. Buffer will only be used as the
    // source for transfer commands (no rendering) via `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`.
    auto stagingBufferInfo = vk::BufferCreateInfo({}, bufferSize, vk::BufferUsageFlagBits::eTransferSrc);
    // Let VMA know that this data should be on CPU RAM.
    auto vmaAllocInfo = vma::AllocationCreateInfo().setUsage(vma::MemoryUsage::eCpuOnly);

    // Allocate the buffer.
    auto stagingBuffer = static_cast<AllocatedBuffer>(allocator->createBuffer(stagingBufferInfo, vmaAllocInfo));

    // To push data into a vk::Buffer, we need to map it first. Mapping a buffer will give us a pointer and, once we are
    // done with writing the data, we can unmap. We copy the mesh vertex data into the buffer.
    void *vertexData;
    vertexData = allocator->mapMemory(stagingBuffer.allocation);
    memcpy(vertexData, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    allocator->unmapMemory(stagingBuffer.allocation);

    // --- GPU-side Buffer Allocation ---
    // Allocate the vertex buffer--we signify that the buffer will be used as vertex buffer so that the driver knows
    // we will use it to render meshes and to copy data into.
    auto vertexBufferInfo = vk::BufferCreateInfo({}, bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    // Let the VMA library know that this data should be GPU native.
    vmaAllocInfo.usage = vma::MemoryUsage::eGpuOnly;

    // Allocate the buffer.
    mesh.vertexBuffer = create_buffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vma::MemoryUsage::eGpuOnly);

    // Execute the copy command, enqueuing a `vkCmdCopyBuffer()` command
    immediate_submit([=, &mesh](vk::CommandBuffer commandBuffer) {
        auto copy = vk::BufferCopy(0, 0, bufferSize);
        commandBuffer.copyBuffer(stagingBuffer.buffer, mesh.vertexBuffer.buffer, {copy});
    });

    // --- Cleanup ---
    // Add the destruction of the triangle mesh buffer to the deletion queue
    mainDeletionQueue.push([=, this]() {
        allocator->destroyBuffer(mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
    });
    allocator->destroyBuffer(stagingBuffer.buffer, stagingBuffer.allocation); // Delete immediately.
}


void VulkanEngine::init_scene() {
    // We create 1 monkey, add it as the first thing to the renderables array, and then we create a lot of triangles in
    // a grid, and put them around the monkey.
    RenderObject monkey = {
        .mesh = get_mesh("monkey"),
        .material = get_material("defaultmesh"),
        .transformMatrix = glm::mat4 {1.0f},
    };
    renderables.push_back(monkey);

    auto translation2 = glm::translate(glm::mat4 {1.0}, glm::vec3(4.5, 1, 0));
    RenderObject fumo = {
        .mesh = get_mesh("fumo"),
        .material = get_material("texturedmesh2"),
        .transformMatrix = translation2,
    };
    renderables.push_back(fumo);

    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {
            auto translation = glm::translate(glm::mat4 {1.0}, glm::vec3(x, 0, y));
            auto scale = glm::scale(glm::mat4 {1.0}, glm::vec3(0.2, 0.2, 0.2));

            RenderObject triangle = {
                .mesh = get_mesh("triangle"),
                .material = get_material("defaultmesh"),
                .transformMatrix = translation * scale,
            };
            renderables.push_back(triangle);
        }
    }

//    RenderObject map = {
//        .mesh = get_mesh("empire"),
//        .material = get_material("texturedmesh"),
//        .transformMatrix = glm::translate(glm::vec3 {5, -10, 0}),
//    };
//    renderables.push_back(map);

    // --- Textures ---
    auto *texturedMaterial = get_material("texturedmesh");
    auto *texturedMaterial2 = get_material("texturedmesh2");
    // Allocate the descriptor set for single-texture to use on the material.
    auto textureDescriptorSetAllocInfo = vk::DescriptorSetAllocateInfo(*descriptorPool, *singleTextureSetLayout);
    texturedMaterial->textureSet = std::move(vk::raii::DescriptorSets(device, textureDescriptorSetAllocInfo).front());
    texturedMaterial2->textureSet = std::move(vk::raii::DescriptorSets(device, textureDescriptorSetAllocInfo).front());

    // Create a sampler for the scene. We want it to be blocky, so we use `VK_FILTER_NEAREST`.
    auto samplerInfo = vkinit::sampler_create_info(vk::Filter::eNearest);
    blockySampler = vk::raii::Sampler(vk::raii::Sampler(device, samplerInfo));

    // Write the descriptor set so that it points to our empire_diffuse texture
    auto imageBufferInfo = vk::DescriptorImageInfo(
        *blockySampler, *loadedTextures["empire_diffuse"].imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
//    auto textureWrite = vkinit::write_descriptor_image(vk::DescriptorType::eCombinedImageSampler, *texturedMaterial->textureSet, &imageBufferInfo, 0);
//    device.updateDescriptorSets({textureWrite}, nullptr);

    imageBufferInfo.imageView = *loadedTextures["fumo_diffuse"].imageView;
    auto textureWrite2 = vkinit::write_descriptor_image(vk::DescriptorType::eCombinedImageSampler, *texturedMaterial2->textureSet, &imageBufferInfo, 0);
    device.updateDescriptorSets({textureWrite2}, nullptr);
}


void VulkanEngine::init_imgui() {
    // --- Create Descriptor Pool for ImGui ---
    // The size of the pool is very oversized, but it doesn't matter
    vk::DescriptorPoolSize poolSizes[] = {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eUniformBufferDynamic, 1000},
        {vk::DescriptorType::eStorageBufferDynamic, 1000},
        {vk::DescriptorType::eInputAttachment, 1000}
    };

    auto poolInfo = vk::DescriptorPoolCreateInfo()
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(1000)
        .setPoolSizeCount(std::size(poolSizes))
        .setPPoolSizes(poolSizes);

    imguiPool = vk::raii::DescriptorPool(device, poolInfo);


    // --- Initialize ImGui Library ---
    ImGui::CreateContext();               // Initialize ImGui core structures
    ImGui_ImplSDL2_InitForVulkan(window); // Initialize ImGui for SDL

    // Initialize ImGui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = *instance,
        .PhysicalDevice = *chosenGPU,
        .Device = *device,
        .Queue = graphicsQueue,
        .DescriptorPool = *imguiPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };
    ImGui_ImplVulkan_Init(&initInfo, *renderpass);

    // Execute a GPU command to upload imgui font textures
    immediate_submit([&](vk::CommandBuffer commandBuffer) {
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    });

    // --- Cleanup ---
    ImGui_ImplVulkan_DestroyFontUploadObjects(); // Clear font textures from CPU data
    mainDeletionQueue.push([=]() {
        ImGui_ImplVulkan_Shutdown();
    });
}


void VulkanEngine::create_material(vk::raii::Pipeline pipeline, vk::raii::PipelineLayout layout, const std::string &name) {
    materials[name] = Material(nullptr, std::move(pipeline), std::move(layout));
}


Material *VulkanEngine::get_material(const std::string &name) {
    // Search for the object; return `nullptr` if not found.
    auto it = materials.find(name);
    return it == materials.end() ? nullptr : &(*it).second;
}


Mesh *VulkanEngine::get_mesh(const std::string &name) {
    // Search for the object; return `nullptr` if not found.
    auto it = meshes.find(name);
    return it == meshes.end() ? nullptr : &(*it).second;
}


void VulkanEngine::draw_objects(const vk::raii::CommandBuffer &commandBuffer, RenderObject *first, uint32_t count) {
    const auto FRAME_OFFSET = static_cast<uint32_t>(pad_uniform_buffer_size(sizeof(GPUCameraData) + sizeof(GPUSceneData)));

    auto &currentFrame = get_current_frame();
    // --- Writing Camera Data (View) ---
    auto view = glm::lookAt(camera, camera + forward, up);
    auto projection = glm::perspective(glm::radians(70.f), 16.f / 9.f, 0.1f, 200.f);
    projection[1][1] *= -1; // Correct OpenGL coordinate system

    // --- Writing Scene Data ---
    auto cameraParameters = GPUCameraData(view, projection, projection * view);
    auto framed = static_cast<float>(animationFrameNumber) / 30.f;
    auto frameIndex = frameNumber % FRAME_OVERLAP;
    sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};
    sceneParameters.sunlightDirection = {sin(framed) * 3.0, 2.0, cos(framed) * 3.0, 0.1};

    uint8_t *sceneData;
    VK_CHECK(allocator->mapMemory(sceneParameterBuffer.allocation, (void **) &sceneData));

    // Write camera parameter data
    sceneData += FRAME_OFFSET * frameIndex;
    memcpy(sceneData, &cameraParameters, sizeof(GPUCameraData));

    // Write scene parameter data
    sceneData += sizeof(GPUCameraData);
    memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));

    allocator->unmapMemory(sceneParameterBuffer.allocation);

    // --- Writing Object Storage Data ---
    void *objectData;
    VK_CHECK(allocator->mapMemory(currentFrame.objectBuffer.allocation, &objectData));

    // Instead of using `memcpy` here, we cast `void*` to another type (Shader Storage Buffer Object) and write to it
    // normally.
    auto *objectSSBO = reinterpret_cast<GPUObjectData *>(objectData);
    for (uint32_t i = 0; i < count; i++) {
        auto &object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }
    allocator->unmapMemory(currentFrame.objectBuffer.allocation);

    // --- Draw Setup ---
    Mesh *lastMesh = nullptr;
    Material *lastMaterial = nullptr;
    for (uint32_t i = 0; i < count; i++) {
        auto &object = first[i];

        // Only bind the pipeline if it doesn't match with the already bound one.
        if (object.material != lastMaterial) {
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *object.material->pipeline);
            lastMaterial = object.material;

            // Bind the camera data descriptor set when changing pipelines.
            // Offset for our scene buffer--dynamic uniform buffers allow us to specify offset when binding.
            auto uniform_offset = FRAME_OFFSET * frameIndex;
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *object.material->pipelineLayout, 0, {*globalDescriptor}, uniform_offset);

            // Bind the object data descriptor set
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *object.material->pipelineLayout, 1, {*currentFrame.objectDescriptor}, nullptr);

            // Binding the texture descriptor set if the texture set handle isn't null.
            if (*object.material->textureSet) {
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *object.material->pipelineLayout, 2, {*object.material->textureSet}, nullptr);
            }
        }

        // Upload the model space mesh matrix to the GPU via push constants.
        MeshPushConstants constants = {.renderMatrix = object.transformMatrix};
        commandBuffer.pushConstants<MeshPushConstants>(
            *object.material->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, constants);

        // Only bind the mesh vertex buffer with offset 0 if it's different from the last bound one.
        if (object.mesh != lastMesh) {
            vk::DeviceSize offset = 0;
            commandBuffer.bindVertexBuffers(0, {object.mesh->vertexBuffer.buffer}, {offset});
            lastMesh = object.mesh;
        }

        // We can now draw the mesh! We pass the index into the `vkCmdDraw` call to send the instance index to the shader.
        commandBuffer.draw(object.mesh->vertices.size(), 1, 0, i);
    }
}

FrameData &VulkanEngine::get_current_frame() {
    // With a frame overlap of 2 (default), it means that even frames will use `frames[0]`, while odd frames will use `frames[1]`.
    // While the GPU is busy executing the rendering commands from frame 0, the CPU will be writing the buffers of frame 1, and reverse.
    return frames[frameNumber % FRAME_OVERLAP];
}


AllocatedBuffer VulkanEngine::create_buffer(size_t size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage) {
    // --- Allocate Vertex Buffer ---
    auto bufferInfo = vk::BufferCreateInfo({}, size, bufferUsage);
    auto vmaAllocInfo = vma::AllocationCreateInfo({}, memoryUsage);

    // Allocate the buffer
    return static_cast<AllocatedBuffer>(allocator->createBuffer(bufferInfo, vmaAllocInfo));
}


void VulkanEngine::init_descriptors() {
    const auto SCENE_BUFFER_SIZE = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUCameraData) + sizeof(GPUSceneData));
    const int MAX_OBJECTS = 10E3;

    // --- Descriptor Pool Setup ---
    // When creating a descriptor pool, you need to specify how many descriptors of each type you will need, and what’s
    // the maximum number of sets to allocate from it.

    // Create a descriptor pool that will hold 10 dynamic uniform buffer handles and 10 storage buffer handles, each
    // with a maximum of 10 descriptor sets.
    std::vector<vk::DescriptorPoolSize> sizes = {
        {vk::DescriptorType::eUniformBufferDynamic, 10},
        {vk::DescriptorType::eStorageBuffer, 10},
        {vk::DescriptorType::eCombinedImageSampler, 10}, // Add combined-image-sampler descriptor types to the pool
    };

    auto descriptorPoolInfo = vk::DescriptorPoolCreateInfo({}, 10, sizes.size(), sizes.data());
    descriptorPool = vk::raii::DescriptorPool(device, descriptorPoolInfo);

    // --- Descriptor Set Layouts ---
    // Global descriptor set
    // Binding for scene data + camera data at 0 and scene data at 1
    auto sceneBinding = vkinit::descriptor_set_layout_binding(
        vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0);
    auto globalSetInfo = vk::DescriptorSetLayoutCreateInfo({}, 1, &sceneBinding);
    globalSetLayout = vk::raii::DescriptorSetLayout(device, globalSetInfo);

    // Object descriptor set
    auto objectBinding = vkinit::descriptor_set_layout_binding(vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 0);
    auto objectSetInfo = vk::DescriptorSetLayoutCreateInfo({}, 1, &objectBinding);
    objectSetLayout = vk::raii::DescriptorSetLayout(device, objectSetInfo);

    // Texture descriptor set which holds a single texture
    auto singleTextureBinding = vkinit::descriptor_set_layout_binding(
        vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 0);
    auto singleTextureSetInfo = vk::DescriptorSetLayoutCreateInfo({}, 1, &singleTextureBinding);
    singleTextureSetLayout = vk::raii::DescriptorSetLayout(device, singleTextureSetInfo);


    // --- Descriptor/Buffer Allocation ---
    auto globalDescriptorSetAllocInfo = vk::DescriptorSetAllocateInfo(*descriptorPool, *globalSetLayout);
    globalDescriptor = std::move(device.allocateDescriptorSets(globalDescriptorSetAllocInfo).front());

    // Uniform buffers are the best for this sort of small, read only shader data. They have a size limitation, but
    // they are very fast to access in the shaders.
    // Due to alignment, we will have to increase the size of the buffer so that it fits two padded `GPUSceneData` and
    // `GPUCameraData` structs.
    sceneParameterBuffer = create_buffer(SCENE_BUFFER_SIZE, vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);

    for (auto &frame : frames) {
        // With storage buffers, you can have an unsized array in a shader with whatever data you want. A common use
        // for them is to store the data of all the objects in the scene.
        frame.objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eCpuToGpu);

        auto objectDescriptorSetAllocInfo = vk::DescriptorSetAllocateInfo(*descriptorPool, *objectSetLayout);
        frame.objectDescriptor = vk::raii::DescriptorSet(std::move(device.allocateDescriptorSets(objectDescriptorSetAllocInfo).front()));

        // We now have a descriptor stored in our frame struct. But this descriptor is not pointing to any buffer yet,
        // so we need to make it point into our buffers.
        auto sceneBufferInfo = vk::DescriptorBufferInfo(sceneParameterBuffer.buffer, 0, sizeof(GPUCameraData) + sizeof(GPUSceneData));
        auto objectBufferInfo = vk::DescriptorBufferInfo(frame.objectBuffer.buffer, 0, sizeof(GPUObjectData) * MAX_OBJECTS);

        auto sceneWrite = vkinit::write_descriptor_buffer(vk::DescriptorType::eUniformBufferDynamic, *globalDescriptor, &sceneBufferInfo, 0);
        auto objectWrite = vkinit::write_descriptor_buffer(vk::DescriptorType::eStorageBuffer, *frame.objectDescriptor, &objectBufferInfo, 0);
        std::array setWrites = {sceneWrite, objectWrite};

        // Note: We use one call to `vkUpdateDescriptorSets()` to update *two* different descriptor sets--this is
        //       completely valid to do!
        device.updateDescriptorSets({sceneWrite, objectWrite}, nullptr);
    }

    // --- Cleanup ---
    mainDeletionQueue.push([this]() {
        // Add buffers to deletion queues
        for (auto &frame : frames) {
            allocator->destroyBuffer(frame.objectBuffer.buffer, frame.objectBuffer.allocation);
        }
        allocator->destroyBuffer(sceneParameterBuffer.buffer, sceneParameterBuffer.allocation);
    });
}


size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize) const {
    // Calculate the required alignment based on minimum device-offset alignment
    size_t minOffsetAlignment = gpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;

    if (minOffsetAlignment > 0)
        alignedSize = (alignedSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
    return alignedSize;
}


void VulkanEngine::immediate_submit(std::function<void(vk::CommandBuffer commandBuffer)> &&function) const {
    // This is similar logic to the render loop (i.e., reusing the same command buffer from frame to frame).
    // If we wanted to submit multiple command buffers, we would simply allocate as many as we needed ahead of time.
    // We first allocate command buffer, we then call the function between begin/end command buffer, and then we submit it.
    // Then we wait for the submit to be finished, and reset the command pool.
    auto commandBuffer = *uploadContext.commandBuffer;

    // Begin the command buffer recording. We will use this command buffer exactly once before resetting.
    auto commandBufferBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(commandBufferBeginInfo);

    // Execute the function
    function(commandBuffer);
    commandBuffer.end();
    auto submitInfo = vkinit::submit_info(&commandBuffer);

    // Submit command buffer to the queue and execute it. `uploadFence` will now block until the graphics commands finish.
    graphicsQueue.submit(submitInfo, *uploadContext.uploadFence);
    VK_CHECK(device.waitForFences({*uploadContext.uploadFence}, true, 1E11));
    device.resetFences({*uploadContext.uploadFence});
    // Reset the command buffers within the command pool.
    uploadContext.commandPool.reset({});
}


void VulkanEngine::cleanup() {
    // SDL is a C library--it does not support constructors and destructors. We have to delete things manually.
    // Note: `vk::PhysicalDevice` and `vk::Queue` cannot be destroyed (they're something akin to handles).
    if (isInitialized) {
        // It is imperative that objects are destroyed in the opposite order that they are created (unless we really
        // know what we are doing)!
        device.waitIdle();

        mainDeletionQueue.flush();

        allocator->destroy();
        SDL_DestroyWindow(window);
    }
}


void VulkanEngine::draw() {
    auto &currentFrame = get_current_frame();
    // --- ImGui ---
    ImGui::Render();

    // --- Setup ---
    // Wait until the GPU has finished rendering the last frame (timeout = 1s)
    VK_CHECK(device.waitForFences({*currentFrame.renderFence}, true, 1E9));
    device.resetFences({*currentFrame.renderFence});

    // Request image from the swapchain (timeout = 1s). We use `presentSemaphore` to make sure that we can sync other
    // operations with the swapchain having an image ready to render.
    uint32_t swapchainImageIndex;
    swapchainImageIndex = swapchain.acquireNextImage(1E9, *currentFrame.presentSemaphore, VK_NULL_HANDLE).second;

    // Commands have finished execution at this point; we can safely reset the command buffer to begin enqueuing again.
    currentFrame.mainCommandBuffer.reset({});

    auto &commandBuffer = currentFrame.mainCommandBuffer;
    // Begin the command buffer recording. As we will use this command buffer exactly once, we want to inform Vulkan
    // to allow for great optimization by the driver.
    auto commandBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(commandBeginInfo);

    // --- Main Renderpass ---
    // Make a clear-color from frame number. This will flash with a 120*π frame period.
    float flash = abs(sin(animationFrameNumber / 120.0f));
    auto clearValue = vk::ClearValue().setColor(vk::ClearColorValue(0.0f, 0.0f, flash, 1.0f));

    vk::ClearValue depthClear;
    depthClear.depthStencil.depth = 1.0f; // Clear depth at z = 1

    vk::ClearValue clearValues[] = {clearValue, depthClear};

    // Start the main renderpass. We will use the clear color from above, and the framebuffer of the index the swapchain
    // gave us.
    auto renderpassInfo = vkinit::renderpass_begin_info(*renderpass, windowExtent, *framebuffers[swapchainImageIndex]);
    renderpassInfo.clearValueCount = 2;
    renderpassInfo.pClearValues = &clearValues[0];

    commandBuffer.beginRenderPass(renderpassInfo, vk::SubpassContents::eInline);

    draw_objects(commandBuffer, renderables.data(), renderables.size());

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);

    commandBuffer.endRenderPass();           // Finalize the renderpass
    commandBuffer.end(); // Finalize the command buffer for execution--we can no longer add commands.

    // Prepare the submission to the queue. We wait on `presentSemaphore`, as it's only signaled when the swapchain is
    // ready. We will then signal `renderSemaphore`, to inform that rendering has finished.
    // clang-format off
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto submitInfo = vkinit::submit_info(&(*commandBuffer))
        .setPWaitDstStageMask(&waitStage)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&(*currentFrame.presentSemaphore))
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&(*currentFrame.renderSemaphore));

    // Submit command buffer to the queue and execute it. `renderFence` will now block until the graphics commands
    // finish execution.
    graphicsQueue.submit({submitInfo}, *currentFrame.renderFence);

    // Put the rendered image into the visible window (after waiting on `renderSemaphore` as it's necessary that
    // drawing commands have finished before the image is displayed to the user).
    auto presentInfo = vkinit::present_info()
        .setSwapchainCount(1)
        .setPSwapchains(&(*swapchain))
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&(*currentFrame.renderSemaphore))
        .setPImageIndices(&swapchainImageIndex);
    // clang-format on

    VK_CHECK(graphicsQueue.presentKHR(presentInfo));

    frameNumber++;
}


void VulkanEngine::run() {
    SDL_Event windowEvent;
    bool shouldQuit = false;
    auto left = glm::cross(up, forward);
    auto mouseSensitivity = 0.005f; // Mouse sensitivity is static, move sensitivity is based on frame time!
    int mouseX, mouseY;

    int lastFrameNumber = frameNumber;
    auto fpsUpdateTimeoutMs = SDL_GetTicks64() + 1000;
    auto animationUpdateTimeoutMs = SDL_GetTicks64() + 17; // ~60Hz

    // Main loop
    while (!shouldQuit) {
        // --- Handle Input ---
        // Handle all events the OS has sent to the application since the last frame.
        while (SDL_PollEvent(&windowEvent) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&windowEvent);
            switch (windowEvent.type) {
                case SDL_KEYDOWN:
//                    switch (windowEvent.key.keysym.sym) {
//                        default:
//                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // Refresh relative mouse coordinates before motion is handled.
                    SDL_GetRelativeMouseState(&mouseX, &mouseY);
                    break;
                case SDL_QUIT:
                    // Close the window when the user presses ALT-F4 or the 'x' button.
                    shouldQuit = true;
                    break;
            }
        }

        // --- Draw ---
        auto startTicksMs = ticksMs;
        // ImGui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // ImGui commands--we can call ImGui functions between `Imgui::NewFrame()` and `draw()`
        ImGui::Begin("VulkanTest2");
        ImGui::Text("FPS: %d", fps);
        ImGui::End();

        draw();

        // --- Update FPS and Animation Timer ---
        ticksMs = SDL_GetTicks64();
        // Update FPS counter
        if (ticksMs >= fpsUpdateTimeoutMs) {
            fps = frameNumber - lastFrameNumber;
            lastFrameNumber = frameNumber;
            fpsUpdateTimeoutMs = ticksMs + 1000; // Update timeout timer.
        }

        if (ticksMs >= animationUpdateTimeoutMs) {
            animationUpdateTimeoutMs = ticksMs + 17;
            animationFrameNumber++;
        }

        // --- Movement Calculations ---
        auto moveSensitivity = 0.25f * (static_cast<float>(ticksMs - startTicksMs) / 17.0f); // This breaks down past 1000Hz!
        // Handle continuously-held key input for movement.
        auto *keyStates = SDL_GetKeyboardState(nullptr);
        camera += static_cast<float>(keyStates[SDL_SCANCODE_W]) * moveSensitivity * forward;
        camera += static_cast<float>(keyStates[SDL_SCANCODE_A]) * moveSensitivity * left;
        camera -= static_cast<float>(keyStates[SDL_SCANCODE_S]) * moveSensitivity * forward;
        camera -= static_cast<float>(keyStates[SDL_SCANCODE_D]) * moveSensitivity * left;
        camera += static_cast<float>(keyStates[SDL_SCANCODE_SPACE]) * glm::vec3(0.f, moveSensitivity, 0.f);
        camera -= static_cast<float>(keyStates[SDL_SCANCODE_LSHIFT]) * glm::vec3(0.f, moveSensitivity, 0.f);

        // Handle held mouse input for mouse movement->camera movement translation.
        if (SDL_GetMouseState(&mouseX, &mouseY) & SDL_BUTTON_LMASK) {
            SDL_GetRelativeMouseState(&mouseX, &mouseY);

            // Calculate rotation matrix
            float angleX = static_cast<float>(mouseX) * -mouseSensitivity;
            float angleY = static_cast<float>(mouseY) * mouseSensitivity;
            glm::mat3 rotate = glm::rotate(glm::mat4(1.0f), angleX, up) * glm::rotate(glm::mat4(1.0f), angleY, left);

            forward = rotate * forward;
            left = glm::cross(up, forward);
        }
    }
}