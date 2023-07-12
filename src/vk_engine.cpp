#include "vk_engine.h"
#include "vk_initializers.h"

#include "VkBootstrap.h"
#include "VkBootstrapDispatch.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <fstream>
#include <future>
#include <iostream>
#include <thread>

// We want to immediately abort when there is an error. In normal engines, this would give an error message to the
// user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                       \
    do {                                                                  \
        VkResult error = x;                                               \
        if (error) {                                                      \
            std::cout << "Detected Vulkan error: " << error << std::endl; \
            abort();                                                      \
        }                                                                 \
    } while (0)

#ifdef NDEBUG
constexpr bool useValidationLayers = false;
#else
constexpr bool useValidationLayers = true;
#endif


VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass renderpass) {
    // Let’s begin by connecting the viewport and scissor into `ViewportState`, and setting the
    // `ColorBlenderStateCreateInfo`.
    // Make viewport state from our stored viewport and scissor. At the moment, we won't support multiple viewports or
    // scissors.
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    // Setup dummy color blending. We aren't using transparent objects yet, so the blending is just set to "no blend",
    // but we do write to the color attachment.
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    // Build the actual pipeline. We will use all the info structs we have been writing into this one for creation.
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .layout = pipelineLayout,
        .renderPass = renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    // It's easy to error when creating the graphics pipeline, so we handle it better than just using VK_CHECK.
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        std::cout << "Failed to create graphics pipeline!\n";
        return VK_NULL_HANDLE;
    }
    return newPipeline;
}


void VulkanEngine::init() {
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
    load_meshes();
    init_scene();

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
    // --- Initialize Vulkan Instance ---
    vkb::InstanceBuilder builder;

    // Make the Vulkan instance, with basic debug features
    auto vkbInstance = builder.set_app_name("VulkanTest2")
                           .request_validation_layers(useValidationLayers)
                           .require_api_version(1, 1, 0)
                           .use_default_debug_messenger()
                           .build()
                           .value();

    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    // --- Initialize Vulkan Device ---
    // Get the surface of the window we opened with SDL
    SDL_Vulkan_CreateSurface(window, instance, &surface);

    // Use `vkBootstrap` to select a GPU. We want a GPU that can write to the SDL surface and supports Vulkan 1.1.
    vkb::PhysicalDeviceSelector selector{vkbInstance};
    auto vkbPhysicalDevice = selector
                                 .set_minimum_version(1, 1)
                                 .set_surface(surface)
                                 .select()
                                 .value();

    // Create the final Vulkan device from the chosen `VkPhysicalDevice`
    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    // Enable the shader draw parameters feature
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };
    auto vkbDevice = deviceBuilder.add_pNext(&shaderDrawParametersFeatures).build().value();

    // Get the `VkDevice` handle used in the rest of the Vulkan application.
    device = vkbDevice.device;
    chosenGPU = vkbPhysicalDevice.physical_device;

    // --- Grabbing Queues ---
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // --- Initialize Memory Allocator ---
    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = chosenGPU,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&allocatorInfo, &allocator);

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
    vkb::SwapchainBuilder swapchainBuilder {chosenGPU, device, surface};

    // --- Present Modes ---
    // VK_PRESENT_MODE_IMMEDIATE_KHR:    Immediate
    // VK_PRESENT_MODE_FIFO_KHR:         Strong VSync
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR: Adaptive VSync (immediate if below target)
    // VK_PRESENT_MODE_MAILBOX_KHR:      Triple-buffering without strong VSync
    auto vkbSwapchain = swapchainBuilder
                            .use_default_format_selection()
                            // An easy way to limit FPS for now.
                            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                            // If you need to resize the window, the swapchain will need to be rebuilt.
                            .set_desired_extent(windowExtent.width, windowExtent.height)
                            .build()
                            .value();

    // Store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;

    mainDeletionQueue.push([this]() { vkDestroySwapchainKHR(device, swapchain, nullptr); });

    // --- Depth Image ---
    VkExtent3D depthImageExtent = {windowExtent.width, windowExtent.height, 1}; // Depth image size matches window

    depthFormat = VK_FORMAT_D32_SFLOAT; // Hard-coding depth format to 32-bit float (most GPUs support it).
    // The depth image will be an image with the format we selected and Depth Attachment usage flag
    auto depthImageInfo = vkinit::image_create_info(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    // For the depth image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo depthImageAllocationInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,                                          // Ensures image is allocated on VRAM.
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // Forces VMA to allocate on VRAM.
    };

    // Allocate and create the image
    vmaCreateImage(allocator, &depthImageInfo, &depthImageAllocationInfo, &depthImage.image, &depthImage.allocation, nullptr);

    // Build an image-view for the depth image to use for rendering
    auto depthViewInfo = vkinit::imageview_create_info(depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));

    mainDeletionQueue.push([this]() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
    });
}


void VulkanEngine::init_commands() {
    // Create a command pool for commands submitted to the graphics queue.
    // We want the pool to allow the resetting of individual command buffers.
    auto commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    auto uploadCommandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily);

    for (auto &frame: frames) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frame.commandPool));

        // Allocate the other default command buffer that we will use for rendering.
        auto commandAllocateInfo = vkinit::command_buffer_allocate_info(frame.commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(device, &commandAllocateInfo, &frame.mainCommandBuffer));

        mainDeletionQueue.push([=, this]() { vkDestroyCommandPool(device, frame.commandPool, nullptr); });
    }

    // Create pool for upload context
    VK_CHECK(vkCreateCommandPool(device, &uploadCommandPoolInfo, nullptr, &uploadContext.commandPool));

    mainDeletionQueue.push([=, this]() { vkDestroyCommandPool(device, uploadContext.commandPool, nullptr); });

    // Allocate the default command buffer that we will use for the instant commands
    auto instantCommandAllocateInfo = vkinit::command_buffer_allocate_info(uploadContext.commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &instantCommandAllocateInfo, &uploadContext.commandBuffer));

}


void VulkanEngine::init_default_renderpass() {
    // Color Attachments are the description of the image we will be writing into with rendering commands.
    VkAttachmentDescription colorAttachment = {
        .format = swapchainImageFormat,          // Attachment will have the format needed by the swapchain
        .samples = VK_SAMPLE_COUNT_1_BIT,        // We will only use 1 sample as we won't be doing MSAA
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,   // We clear when this attachment is loaded
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // We keep the attachment stored when the renderpass ends
        // We don't care about stencils
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

        // We don't know (or care) about the starting layout of the attachment
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        // After the renderpass ends, the image has to be on a layout ready for display.
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentReference = {
        .attachment = 0, // Attachment number will index into the `pAttachments` array in the parent renderpass
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depthAttachment = {
        .flags = 0,
        .format = depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthAttachmentReference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // We are going to create 1 subpass (the minimum amount possible)
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentReference,       // Hook the color attachment into the subpass
        .pDepthStencilAttachment = &depthAttachmentReference, // Hook the depth attachment into the subpass
    };

    VkSubpassDependency colorDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    // We need to make sure that one frame cannot override a depth buffer while a previous frame is still rendering to
    // it. This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before previous
    // renderpasses have finished using it.
    VkSubpassDependency depthDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    // The image life will go something like this:
    //   * <undefined> -> renderpass begins -> sub-pass 0 begins (transition to attachment optimal)
    //                 -> sub-pass 0 renders -> sub-pass 0 ends
    //                 -> renderpass ends (transition to present source)
    VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
    VkSubpassDependency dependencies[2] = {colorDependency, depthDependency};
    // The Vulkan driver will perform the layout transitions for us when using the renderpass. If we weren't using a
    // renderpass (drawing from compute shaders), we would need to do the same transitions explicitly.
    VkRenderPassCreateInfo renderpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = &attachments[0],
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = &dependencies[0],
    };
    VK_CHECK(vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderpass));

    mainDeletionQueue.push([this]() { vkDestroyRenderPass(device, renderpass, nullptr); });
}


void VulkanEngine::init_framebuffers() {
    // Create the framebuffers for the swapchain images. This will connect the renderpass to the images for rendering.
    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(renderpass, windowExtent);

    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    // Create framebuffers for each of the swapchain image views. We connect the depth image view when creating each of
    // the framebuffers. We do not need to change the depth image between frames and can just clear and reuse the same
    // depth image for every.
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[2] = {swapchainImageViews[i], depthImageView};
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]));

        mainDeletionQueue.push([=, this]() {
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        });
    }
}


void VulkanEngine::init_sync_structures() {
    // We want to create the fence with the `CREATE_SIGNALED` flag, so we can wait on it before using it on a
    // GPU command (for the first frame)
    auto fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    auto semaphoreCreateInfo = vkinit::semaphore_create_info();
    auto uploadFenceCreateInfo = vkinit::fence_create_info();

    VK_CHECK(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadContext.uploadFence));
    mainDeletionQueue.push([=, this]() { vkDestroyFence(device, uploadContext.uploadFence, nullptr); });

    for (auto &frame: frames) {
        // --- Create Fence ---
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFence));

        // Enqueue the destruction of the fence
        mainDeletionQueue.push([=, this]() { vkDestroyFence(device, frame.renderFence, nullptr); });

        // --- Create Semaphores ---
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.presentSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore));

        // Enqueue the destruction of semaphores
        mainDeletionQueue.push([=, this]() {
            vkDestroySemaphore(device, frame.presentSemaphore, nullptr);
            vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
        });
    }
}


bool VulkanEngine::load_shader_module(const char *filePath, VkShaderModule *outShaderModule) const {
    // Open the file with cursor at the end. We use `std::ios::binary` to open the stream in binary and `std::ios::ate`
    // to open the stream at the end of the file.
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return false;

    // Find what the size of the file is by looking up the location of the cursor.
    // Because the cursor is at the end, it gives the size directly in bytes.
    auto fileSize = static_cast<std::streamsize>(file.tellg());

    // SPIR-V expects the buffer to be a uint32_t, so make sure to reserve an int vector big enough for the entire file.
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);                               // Put file cursor at the beginning
    file.read((char *) buffer.data(), fileSize); // Load the entire file into the buffer
    file.close();                                // Close file after loading

    // Create a new shader module using the loaded buffer.
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = buffer.size() * sizeof(uint32_t), // `codeSize` has to be in bytes
        .pCode = buffer.data()};

    VkShaderModule shaderModule;
    // Check if the shader module creation goes well. It's very common to have errors that will fail creation, so we
    // will not use `VK_CHECK` here.
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        return false;
    *outShaderModule = shaderModule;
    return true;
}


void VulkanEngine::init_pipelines() {
    // --- Pipeline Layout ---
    VkPipelineLayout meshPipelineLayout;
    // Build the pipeline layout that controls the inputs/outputs of the shader.
    // We are not using descriptor sets or other systems yet, so no need to use anything other than empty default.
    auto meshPipelineLayoutInfo = vkinit::pipeline_layout_create_info();

    // Setup push constants
    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // Push constant range is accessible only in the vertex shader
        .offset = 0,                              // Push constant range starts at the beginning
        .size = sizeof(MeshPushConstants),        // Push constant range has size of `MeshPushConstants` struct
    };

    meshPipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    meshPipelineLayoutInfo.pushConstantRangeCount = 1;

    // Hook the global set layout--we need to let the pipeline know what descriptors will be bound to it.
    VkDescriptorSetLayout setLayouts[] = {globalSetLayout, objectSetLayout};
    meshPipelineLayoutInfo.setLayoutCount = 2;
    meshPipelineLayoutInfo.pSetLayouts = setLayouts;

    VK_CHECK(vkCreatePipelineLayout(device, &meshPipelineLayoutInfo, nullptr, &meshPipelineLayout));

    // --- General Pipeline ---
    PipelineBuilder pipelineBuilder;
    // Vertex input controls how to read vertices from vertex buffers. We aren't using it yet.
    pipelineBuilder.vertexInputInfo = vkinit::vertex_input_state_create_info();

    // Input assembly is the configuration for drawing triangle lists, strips, or individual points. We are just going
    // to draw a triangle list.
    pipelineBuilder.inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // Build viewport and scissor from the swapchain extents.
    pipelineBuilder.viewport.x = 0.0f;
    pipelineBuilder.viewport.y = 0.0f;
    pipelineBuilder.viewport.width = static_cast<float>(windowExtent.width);
    pipelineBuilder.viewport.height = static_cast<float>(windowExtent.height);
    pipelineBuilder.viewport.minDepth = 0.0f;
    pipelineBuilder.viewport.maxDepth = 1.0f;

    pipelineBuilder.scissor.offset = {0, 0};
    pipelineBuilder.scissor.extent = windowExtent;

    // Configure the rasterizer to draw filled triangles.
    pipelineBuilder.rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
    // We don't use multisampling, so just run the default one.
    pipelineBuilder.multisampling = vkinit::multisampling_state_create_info();
    // We use a single blend attachment with no blending and writing to RGBA.
    pipelineBuilder.colorBlendAttachment = vkinit::color_blend_attachment_state();
    // Use the mesh pipeline layout we created.
    pipelineBuilder.pipelineLayout = meshPipelineLayout;
    // Default depth testing
    pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);


    // --- Shader Modules ---
    std::unordered_map<std::string, VkShaderModule> shaderModules;
    std::string shaderBaseDirectory = "../shaders/";
    std::string shaderNames[] = {
        "default_lit.frag",
        "tri_mesh.vert",
    };

    for (const auto &shaderName: shaderNames) {
        VkShaderModule shaderModule;
        if (!load_shader_module((shaderBaseDirectory + shaderName + ".spv").c_str(), &shaderModule)) {
            std::cout << "ERROR: Could not load shader module: " + shaderName << std::endl;
            continue;
        } else {
            std::cout << "Successfully loaded shader module: " + shaderName << std::endl;
            shaderModules[shaderName] = shaderModule;
        }
    }


    // --- Mesh Pipeline ---
    auto vertexDescription = Vertex::get_vertex_description();

    // Connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

    pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shaderModules["tri_mesh.vert"]));
    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shaderModules["default_lit.frag"]));

    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(device, renderpass);
    // Now our mesh pipeline has the space for the push constants, so we can now execute the command to use them.
    create_material(meshPipeline, meshPipelineLayout, "defaultmesh");

    //    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder
    //    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shaderModules["tri_mesh.vert"]));
    //    pipelineBuilder.shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shaderModules["triangle.frag"]));
    //
    //    VkPipeline testPipeline = pipelineBuilder.build_pipeline(device, renderpass);
    //    create_material(testPipeline, meshPipelineLayout, "testmesh");

    // --- Cleanup ---
    // Destroy all shader modules, outside the queue
    for (const auto &[shaderName, shaderModule]: shaderModules)
        vkDestroyShaderModule(device, shaderModule, nullptr);

    mainDeletionQueue.push([=, this]() {
        // Destroy the pipelines we have created.
        vkDestroyPipeline(device, meshPipeline, nullptr);
        //        vkDestroyPipeline(device, testPipeline, nullptr);

        // Destroy the pipeline layouts that they use.
        vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
    });
}


void VulkanEngine::load_meshes() {
    Mesh triangleMesh, monkeyMesh, fumoMesh;
    triangleMesh.vertices.resize(3);

    triangleMesh.vertices[0].position = {1.f, 1.f, 0.f};
    triangleMesh.vertices[1].position = {-1.f, 1.f, 0.f};
    triangleMesh.vertices[2].position = {0.f, -1.f, 0.f};

    triangleMesh.vertices[0].color = {0.f, 1.f, 0.f};
    triangleMesh.vertices[1].color = {0.f, 1.f, 0.f};
    triangleMesh.vertices[2].color = {0.f, 1.f, 0.f};

    monkeyMesh.load_from_obj("../assets/monkey_smooth.obj", "../assets");
    fumoMesh.load_from_obj("../assets/cirno_low.obj", "../assets");

    // We need to make sure both meshes are sent to the GPU. We don't care about vertex normals.
    upload_mesh(triangleMesh);
    upload_mesh(monkeyMesh);
    upload_mesh(fumoMesh);

    // Note: that we are copying them. Eventually we will delete the hardcoded `monkey` and `triangle` meshes, so it's
    //       no problem now.
    meshes["triangle"] = triangleMesh;
    meshes["monkey"] = monkeyMesh;
    meshes["fumo"] = fumoMesh;
}


void VulkanEngine::upload_mesh(Mesh &mesh) {
    const auto bufferSize = mesh.vertices.size() * sizeof(Vertex);
    VmaAllocationCreateInfo vmaAllocInfo = {};

    // --- CPU-side Buffer Allocation ---
    // Create staging buffer to hold the mesh data before uploading to GPU buffer. Buffer will only be used as the
    // source for transfer commands (no rendering) via `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`.
    VkBufferCreateInfo stagingBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    // Let VMA know that this data should be on CPU RAM.
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBuffer stagingBuffer{};
    // Allocate the buffer.
    VK_CHECK(vmaCreateBuffer(allocator, &stagingBufferInfo, &vmaAllocInfo,
                             &stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

    // To push data into a VkBuffer, we need to map it first. Mapping a buffer will give us a pointer and, once we are
    // done with writing the data, we can unmap. We copy the mesh vertex data into the buffer.
    void *vertexData;
    vmaMapMemory(allocator, stagingBuffer.allocation, &vertexData);
    memcpy(vertexData, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, stagingBuffer.allocation);

    // --- GPU-side Buffer Allocation ---
    // Allocate the vertex buffer--we signify that the buffer will be used as vertex buffer so that the driver knows
    // we will use it to render meshes and to copy data into.
    VkBufferCreateInfo vertexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    // Let the VMA library know that this data should be GPU native.
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    // Allocate the buffer.
    VK_CHECK(vmaCreateBuffer(allocator, &vertexBufferInfo, &vmaAllocInfo,
                             &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    // Execute the copy command, enqueuing a `vkCmdCopyBuffer()` command
    immediate_submit([=](VkCommandBuffer commandBuffer) {
        VkBufferCopy copy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bufferSize,
        };
        vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
    });

    // Add the destruction of the triangle mesh buffer to the deletion queue
    mainDeletionQueue.push([=, this]() {
        vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
    });
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation); // Delete immediately.
}


void VulkanEngine::init_scene() {
    // We create 1 monkey, add it as the first thing to the renderables array, and then we create a lot of triangles in
    // a grid, and put them around the monkey.
    RenderObject monkey = {
        .mesh = get_mesh("monkey"),
        .material = get_material("defaultmesh"),
        .transformMatrix = glm::mat4{1.0f},
    };
    renderables.push_back(monkey);


    auto translation2 = glm::translate(glm::mat4{1.0}, glm::vec3(3, 0, 0));
    auto scale2 = glm::scale(glm::mat4{1.0}, glm::vec3(0.1, 0.1, 0.1));
    RenderObject fumo = {
        .mesh = get_mesh("fumo"),
        .material = get_material("defaultmesh"),
        .transformMatrix = translation2 * scale2,
    };
    renderables.push_back(fumo);

    for (int x = -20; x <= 20; x++)
        for (int y = -20; y <= 20; y++) {
            auto translation = glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y));
            auto scale = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));

            RenderObject triangle = {
                .mesh = get_mesh("triangle"),
                .material = get_material("defaultmesh"),
                .transformMatrix = translation * scale,
            };
            renderables.push_back(triangle);
        }
}


Material *VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name) {
    materials[name] = {pipeline, layout};
    return &materials[name];
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


void VulkanEngine::draw_objects(VkCommandBuffer commandBuffer, RenderObject *first, uint32_t count) {
    auto currentFrame = get_current_frame();
    // --- Writing Camera Data (View) ---
    auto view = glm::lookAt(camera, camera + forward, up);
    auto projection = glm::perspective(glm::radians(70.f), 16.f / 9.f, 0.1f, 200.f);
    projection[1][1] *= -1; // Correct OpenGL coordinate system

    GPUCameraData cameraData = {view, projection, projection * view};

    void *bufferData;
    vmaMapMemory(allocator, currentFrame.cameraBuffer.allocation, &bufferData);
    memcpy(bufferData, &cameraData, sizeof(GPUCameraData));
    vmaUnmapMemory(allocator, currentFrame.cameraBuffer.allocation);

    // --- Writing Scene Data ---
    auto framed = static_cast<float>(animationFrameNumber) / 120.f;
    auto frameIndex = frameNumber % FRAME_OVERLAP;
    sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};

    uint8_t *sceneData;
    vmaMapMemory(allocator, sceneParameterBuffer.allocation, (void **) &sceneData);

    sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
    memcpy(sceneData, &sceneParameters, sizeof(GPUSceneData));
    vmaUnmapMemory(allocator, sceneParameterBuffer.allocation);

    // --- Writing Object Storage Data ---
    void *objectData;
    vmaMapMemory(allocator, currentFrame.objectBuffer.allocation, &objectData);

    // Instead of using `memcpy` here, we cast `void*` to another type (Shader Storage Buffer Object) and write to it
    // normally.
    auto *objectSSBO = reinterpret_cast<GPUObjectData *>(objectData);
    for (int i = 0; i < count; i++) {
        RenderObject &object = first[i];
        objectSSBO[i].modelMatrix = object.transformMatrix;
    }
    vmaUnmapMemory(allocator, currentFrame.objectBuffer.allocation);

    // --- Draw Setup ---
    Mesh *lastMesh = nullptr;
    Material *lastMaterial = nullptr;
    for (int i = 0; i < count; i++) {
        RenderObject &object = first[i];

        // Only bind the pipeline if it doesn't match with the already bound one.
        if (object.material != lastMaterial) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
            lastMaterial = object.material;

            // Bind the camera data descriptor set when changing pipelines.
            // Offset for our scene buffer--dynamic uniform buffers allow us to specify offset when binding.
            uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &currentFrame.globalDescriptor, 1, &uniform_offset);

            // Bind the object data descriptor set
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &currentFrame.objectDescriptor, 0, nullptr);
        }

        // Upload the model space mesh matrix to the GPU via push constants.
        MeshPushConstants constants = {.renderMatrix = object.transformMatrix};
        vkCmdPushConstants(commandBuffer, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        // Only bind the mesh vertex buffer with offset 0 if it's different from the last bound one.
        if (object.mesh != lastMesh) {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
            lastMesh = object.mesh;
        }

        // We can now draw the mesh! We pass the index into the `vkCmdDraw` call to send the instance index to the shader.
        vkCmdDraw(commandBuffer, object.mesh->vertices.size(), 1, 0, i);
    }
}

FrameData &VulkanEngine::get_current_frame() {
    // With a frame overlap of 2 (default), it means that even frames will use `frames[0]`, while odd frames will use `frames[1]`.
    // While the GPU is busy executing the rendering commands from frame 0, the CPU will be writing the buffers of frame 1, and reverse.
    return frames[frameNumber % FRAME_OVERLAP];
}


AllocatedBuffer VulkanEngine::create_buffer(size_t size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage) const {
    // --- Allocate Vertex Buffer ---
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = bufferUsage,
    };

    VmaAllocationCreateInfo vmaAllocInfo = {
        .usage = memoryUsage,
    };

    // Allocate the buffer
    AllocatedBuffer newBuffer{};
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, nullptr));

    return newBuffer;
}


void VulkanEngine::init_descriptors() {
    const size_t SCENE_BUFFER_SIZE = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));
    const int MAX_OBJECTS = 10E3;

    // --- Descriptor Pool Setup ---
    // When creating a descriptor pool, you need to specify how many descriptors of each type you will need, and what’s
    // the maximum number of sets to allocate from it.

    // Create a descriptor pool that will hold 10 uniform buffer handles and 10 dynamic uniform buffer handles, each
    // with a maximum of 10 descriptor sets.
    std::vector<VkDescriptorPoolSize> sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = 10,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };
    vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);

    // --- Descriptor Set Layouts ---
    // Global descriptor set
    // Binding for camera data at 0 and scene data at 1
    auto cameraBinding = vkinit::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    auto sceneBinding = vkinit::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    VkDescriptorSetLayoutBinding bindings[] = {cameraBinding, sceneBinding};

    VkDescriptorSetLayoutCreateInfo globalDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    vkCreateDescriptorSetLayout(device, &globalDescriptorSetInfo, nullptr, &globalSetLayout);

    // Object descriptor set
    auto objectBinding = vkinit::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

    VkDescriptorSetLayoutCreateInfo objectDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &objectBinding,
    };
    vkCreateDescriptorSetLayout(device, &objectDescriptorSetInfo, nullptr, &objectSetLayout);

    // --- Descriptor/Buffer Allocation ---
    // Due to alignment, we will have to increase the size of the buffer so that it fits 2 padded `GPUSceneData` structs
    sceneParameterBuffer = create_buffer(SCENE_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    for (auto &frame: frames) {
        // Uniform buffers are the best for this sort of small, read only shader data. They have a size limitation, but
        // they are very fast to access in the shaders.
        frame.cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame.objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetAllocateInfo globalDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &globalSetLayout,
        };
        vkAllocateDescriptorSets(device, &globalDescriptorSetAllocInfo, &frame.globalDescriptor);

        VkDescriptorSetAllocateInfo objectDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &objectSetLayout,
        };
        vkAllocateDescriptorSets(device, &objectDescriptorSetAllocInfo, &frame.objectDescriptor);

        // We now have a descriptor stored in our frame struct. But this descriptor is not pointing to any buffer yet,
        // so we need to make it point into our camera buffer.
        VkDescriptorBufferInfo cameraBufferInfo = {
            .buffer = frame.cameraBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData),
        };

        VkDescriptorBufferInfo sceneBufferInfo = {
            .buffer = sceneParameterBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUSceneData),
        };

        VkDescriptorBufferInfo objectBufferInfo = {
            .buffer = frame.objectBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUObjectData) * MAX_OBJECTS,
        };

        auto cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame.globalDescriptor, &cameraBufferInfo, 0);
        auto sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, frame.globalDescriptor, &sceneBufferInfo, 1);
        auto objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame.objectDescriptor, &objectBufferInfo, 0);
        VkWriteDescriptorSet setWrites[] = {cameraWrite, sceneWrite, objectWrite};

        // Note: We use one call to `vkUpdateDescriptorSets()` to update *two* different descriptor sets--this is
        //       completely valid to do!
        vkUpdateDescriptorSets(device, 3, setWrites, 0, nullptr);
    }

    // --- Cleanup ---
    mainDeletionQueue.push([&]() {
        // Add descriptor set layout to deletion queues
        vkDestroyDescriptorSetLayout(device, globalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, objectSetLayout, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        // Add buffers to deletion queues
        for (auto &frame: frames) {
            vmaDestroyBuffer(allocator, frame.cameraBuffer.buffer, frame.cameraBuffer.allocation);
            vmaDestroyBuffer(allocator, frame.objectBuffer.buffer, frame.objectBuffer.allocation);
        }
        vmaDestroyBuffer(allocator, sceneParameterBuffer.buffer, sceneParameterBuffer.allocation);
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


void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer commandBuffer)> &&function) {
    // This is similar logic to the render loop (i.e., reusing the same command buffer from frame to frame).
    // If we wanted to submit multiple command buffers, we would simply allocate as many as we needed ahead of time.

    // We first allocate command buffer, we then call the function between begin/end command buffer, and then we submit it.
    // Then we wait for the submit to be finished, and reset the command pool.
    auto commandBuffer = uploadContext.commandBuffer;

    // Begin the command buffer recording. We will use this command buffer exactly once before resetting.
    auto commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // Execute the function
    function(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    auto submitInfo = vkinit::submit_info(&commandBuffer);
    // Submit command buffer to the queue and execute it. `uploadFence` will now block until the graphics commands finish.
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadContext.uploadFence));

    vkWaitForFences(device, 1, &uploadContext.uploadFence, true, 1E11);
    vkResetFences(device, 1, &uploadContext.uploadFence);

    // Reset the command buffers within the command pool.
    vkResetCommandPool(device, uploadContext.commandPool, 0);
}


void VulkanEngine::cleanup() {
    // SDL is a C library--it does not support constructors and destructors. We have to delete things manually.
    // Note: `VkPhysicalDevice` and `VkQueue` cannot be destroyed (they're something akin to handles).
    if (isInitialized) {
        // It is imperative that objects are destroyed in the opposite order that they are created (unless we really
        // know what we are doing)!
        vkDeviceWaitIdle(device);

        mainDeletionQueue.flush();

        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
    }
}


void VulkanEngine::draw() {
    auto currentFrame = get_current_frame();
    // --- Setup ---
    // Wait until the GPU has finished rendering the last frame (timeout = 1s)
    VK_CHECK(vkWaitForFences(device, 1, &currentFrame.renderFence, true, 1E9));
    VK_CHECK(vkResetFences(device, 1, &currentFrame.renderFence));

    // Request image from the swapchain (timeout = 1s). We use `presentSemaphore` to make sure that we can sync other
    // operations with the swapchain having an image ready to render.
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1E9, currentFrame.presentSemaphore, nullptr, &swapchainImageIndex));

    // Commands have finished execution at this point; we can safely reset the command buffer to begin enqueuing again.
    VK_CHECK(vkResetCommandBuffer(currentFrame.mainCommandBuffer, 0));

    auto commandBuffer = currentFrame.mainCommandBuffer;
    // Begin the command buffer recording. As we will use this command buffer exactly once, we want to inform Vulkan
    // to allow for great optimization by the driver.
    VkCommandBufferBeginInfo commandBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBeginInfo));

    // --- Main Renderpass ---
    // Make a clear-color from frame number. This will flash with a 120*π frame period.
    VkClearValue clearValue;
    float flash = abs(sin(static_cast<float>(animationFrameNumber) / 120.0f));
    clearValue.color = {{0.0f, 0.0f, flash, 1.0f}};

    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.0f; // Clear depth at z = 1

    VkClearValue clearValues[] = {clearValue, depthClear};

    // Start the main renderpass. We will use the clear color from above, and the framebuffer of the index the swapchain
    // gave us.
    VkRenderPassBeginInfo renderpassInfo = vkinit::renderpass_begin_info(renderpass, windowExtent, framebuffers[swapchainImageIndex]);
    renderpassInfo.clearValueCount = 2;
    renderpassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    draw_objects(commandBuffer, renderables.data(), renderables.size());

    vkCmdEndRenderPass(commandBuffer);           // Finalize the renderpass
    VK_CHECK(vkEndCommandBuffer(commandBuffer)); // Finalize the command buffer for execution--we can no longer add commands.

    // Prepare the submission to the queue. We wait on `presentSemaphore`, as it's only signaled when the swapchain is
    // ready. We will then signal `renderSemaphore`, to inform that rendering has finished.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = vkinit::submit_info(&commandBuffer);
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &currentFrame.presentSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &currentFrame.renderSemaphore;

    // Submit command buffer to the queue and execute it. `renderFence` will now block until the graphics commands
    // finish execution.
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFrame.renderFence));

    // Put the rendered image into the visible window (after waiting on `renderSemaphore` as it's necessary that
    // drawing commands have finished before the image is displayed to the user).
    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &currentFrame.renderSemaphore;
    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

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
        // Handle all events the OS has sent to the application since the last frame.
        while (SDL_PollEvent(&windowEvent) != 0) {
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

        auto startTicksMs = ticksMs;

        draw();

        ticksMs = SDL_GetTicks64();
        auto frameTimeMs = ticksMs - startTicksMs;
        auto moveSensitivity = 0.25f * (static_cast<float>(frameTimeMs) / 17.0f); // This breaks down past 1000Hz!

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

        // Update FPS counter
        if (ticksMs >= fpsUpdateTimeoutMs) {
            auto fps = frameNumber - lastFrameNumber;
            auto windowTitle = std::string("VulkanTest2") + (useValidationLayers ? " (DEBUG)" : "") + " (FPS: " + std::to_string(fps) + ")";
            SDL_SetWindowTitle(window, windowTitle.c_str());

            lastFrameNumber = frameNumber;
            fpsUpdateTimeoutMs = ticksMs + 1000; // Update timeout timer.
        }

        if (ticksMs >= animationUpdateTimeoutMs) {
            animationUpdateTimeoutMs = ticksMs + 17;
            animationFrameNumber++;
        }
    }
}