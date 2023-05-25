#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

// --- other includes ---
#include <vk_types.h>
#include <vk_initializers.h>

// Bootstrap library
#include "VkBootstrap.h"
#include "VkBootstrapDispatch.h"

#include <iostream>
#include <fstream>

// We want to immediately abort when there is an error. In normal engines this would give an error message to the
// user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                       \
    do {                                                                  \
        VkResult error = x;                                               \
        if (error) {                                                      \
            std::cout << "Detected Vulkan error: " << error << std::endl; \
            abort();                                                      \
        }                                                                 \
    } while (0)                                                           \

#ifdef NDEBUG
    const bool useValidationLayers = false;
#else
    const bool useValidationLayers = true;
#endif


class PipelineBuilder {
public:
    // This is a basic set of required Vulkan structs for pipeline creation, there are more, but for now these are the
    // ones we will need to fill for now.
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineVertexInputStateCreateInfo         vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo       inputAssembly;
    VkViewport                                   viewport;
    VkRect2D                                     scissor;
    VkPipelineRasterizationStateCreateInfo       rasterizer;
    VkPipelineColorBlendAttachmentState          colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo         multisampling;
    VkPipelineLayout                             pipelineLayout;

    VkPipeline build_pipeline(VkDevice device, VkRenderPass renderpass);
};

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
            .pColorBlendState = &colorBlending,
            .layout = pipelineLayout,
            .renderPass = renderpass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
    };

    // It's easy to error when creating the graphics pipeline, so we handle it a better than just using VK_CHECK.
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        std::cout << "Failed to create graphics pipeline!\n";
        return VK_NULL_HANDLE;
    }
    return newPipeline;
}


class VulkanEngine {
public:
    // Initializes everything in the engine.
    void init();

    // Shuts down the engine.
    void cleanup();

    // Draw loop.
    void draw();

    // Run main loop.
    void run();


public:
    // --- Window ---
    bool               isInitialized{false};
    int                frameNumber{0};
    VkExtent2D         windowExtent{1280, 800}; // The width and height of the window (px)
    struct SDL_Window *window{nullptr};         // Forward-declaration for the window

    // --- Vulkan ---
    VkInstance               instance;       // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger; // Vulkan debug output handle
    VkPhysicalDevice         chosenGPU;      // GPU chosen as the default device
    VkDevice                 device;         // Vulkan device for commands
    VkSurfaceKHR             surface;        // Vulkan window surface

    // --- Swapchain ---
    VkSwapchainKHR           swapchain;
    VkFormat                 swapchainImageFormat; // Image format expected by the windowing system
    std::vector<VkImage>     swapchainImages;      // Array of images from the swapchain
    std::vector<VkImageView> swapchainImageViews;  // Array of image-views from the swapchain

    // --- Commands ---
    VkQueue         graphicsQueue;       // The queue we will submit to.
    uint32_t        graphicsQueueFamily; // The family of said queue.
    VkCommandPool   commandPool;         // The command pool for our commands.
    VkCommandBuffer mainCommandBuffer;   // The buffer we will record into.

    // --- Renderpass ---
    VkRenderPass               renderPass;
    std::vector<VkFramebuffer> framebuffers;

    // --- Structure Synchronization ---
    VkSemaphore presentSemaphore, renderSemaphore;
    VkFence     renderFence;

    // --- Pipeline ---
    VkPipelineLayout trianglePipelineLayout;
    VkPipeline       trianglePipeline;

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    // Loads a shader module from a spir-v file. Returns false if it errors
    bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

    void init_pipelines();
};

void VulkanEngine::init() {
    // We initialize SDL and create a window with it. `SDL_INIT_VIDEO` tells SDL that we want the main windowing
    // functionality (includes basic input events like keys or mouse).
    SDL_Init(SDL_INIT_VIDEO);

    auto windowFlags = (SDL_WindowFlags) (SDL_WINDOW_VULKAN);
    auto windowTitle = std::string("VulkanTest2") + (useValidationLayers ? " (DEBUG)" : "");
    // Create blank SDL window for our application
    window = SDL_CreateWindow(
        windowTitle.c_str(),                   // Window title
        SDL_WINDOWPOS_UNDEFINED,               // Window x position (don't care)
        SDL_WINDOWPOS_UNDEFINED,               // Window y position (don't care)
        static_cast<int>(windowExtent.width),  // Window height (px)
        static_cast<int>(windowExtent.height), // Window width  (px)
        windowFlags
    );

    init_vulkan();
    init_swapchain();
    init_commands();
    init_default_renderpass();
    init_framebuffers();
    init_sync_structures();
    init_pipelines();

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
            .build().value();

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
            .select().value();

    // Create the final Vulkan device from the chosen `VkPhysicalDevice`
    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    auto vkbDevice = deviceBuilder.build().value();

    // Get the `VkDevice` handle used in the rest of the Vulkan application.
    device = vkbDevice.device;
    chosenGPU = vkbPhysicalDevice.physical_device;

    // --- Grabbing Queues ---
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}


void VulkanEngine::init_swapchain() {
    vkb::SwapchainBuilder swapchainBuilder{chosenGPU, device, surface};

    // --- Present Modes ---
    // VK_PRESENT_MODE_IMMEDIATE_KHR:    Immediate
    // VK_PRESENT_MODE_FIFO_KHR:         Strong VSync
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR: Adaptive VSync (immediate if below target)
    // VK_PRESENT_MODE_MAILBOX_KHR:      Triple-buffering without string VSync
    auto vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
            // An easy way to limit FPS for now.
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            // If you need to resize the window, the swapchain will need to be rebuilt.
            .set_desired_extent(windowExtent.width, windowExtent.height)
            .build().value();

    // Store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;
}


void VulkanEngine::init_commands() {
    // Create a command pool for commands submitted to the graphics queue.
    // We want the pool to allow the resetting of individual command buffers.
    auto commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    // Allocate the other default command buffer that we will use for rendering.
    auto commandAllocateInfo = vkinit::command_buffer_allocate_info(commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &commandAllocateInfo, &mainCommandBuffer));
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

    // We are going to create 1 subpass (the minimum amount possible)
    VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentReference,
    };

    // The image life will go something like this:
    //   * <undefined> -> renderpass begins -> sub-pass 0 begins (transition to attachment optimal)
    //                 -> sub-pass 0 renders -> sub-pass 0 ends
    //                 -> renderpass ends (transition to present source)
    // The Vulkan driver will perform the layout transitions for us when using the renderpass. If we weren't using a
    // renderpass (drawing from compute shaders) we would need to do the same transitions explicitly.
    VkRenderPassCreateInfo renderpassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment, // Connect color attachment
            .subpassCount = 1,
            .pSubpasses = &subpass,           // Connect subpass
    };
    VK_CHECK(vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderPass));
}


void VulkanEngine::init_framebuffers() {
    // Create the framebuffers for the swapchain images. This will connect the renderpass to the images for rendering.
    VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .width = windowExtent.width,
            .height = windowExtent.height,
            .layers = 1,
    };

    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    // Create framebuffers for each of the swapchain image views
    for (int i = 0; i < swapchainImageCount; i++) {
        framebufferInfo.pAttachments = &swapchainImageViews[i];
        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]));
    }
}


void VulkanEngine::init_sync_structures() {
    // --- Create Fence ---
    VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            // We want to create the fence with the `CREATE_SIGNALED` flag, so we can wait on it before using it on a
            // GPU command (for the first frame)
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));

    // --- Create Semaphores ---
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0, // No flags needed for semaphores
    };
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSemaphore));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore));
}


bool VulkanEngine::load_shader_module(const char *filePath, VkShaderModule *outShaderModule) {
    // Open the file with cursor at the end. We use `std::ios::binary` to open the stream in binary and `std::ios::ate`
    // to open the stream at the end of the file.
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return false;

    // Find what the size of the file is by looking up the location of the cursor.
    // Because the cursor is at the end, it gives the size directly in bytes.
    auto fileSize = static_cast<std::streamsize>(file.tellg());

    // SPIR-V expects the buffer to be a uint32_t, so make sure to reserve an int vector big enough for the entire file.
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);                              // Put file cursor at the beginning
    file.read((char *)buffer.data(), fileSize); // Load the entire file into the buffer
    file.close();                               // Close file after loading

    // Create a new shader module using the loaded buffer.
    VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .codeSize = buffer.size() * sizeof(uint32_t), // `codeSize` has to be in bytes
            .pCode = buffer.data()
    };

    VkShaderModule shaderModule;
    // Check if the shader module creation goes well. It's very common to have errors that will fail creation, so we
    // will not use `VK_CHECK` here.
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        return false;
    *outShaderModule = shaderModule;
    return true;
}


void VulkanEngine::init_pipelines() {
    // --- Shader Module Loading ---
    VkShaderModule triangleVertexShader;
    if (!load_shader_module("../shaders/triangle.vert.spv", &triangleVertexShader))
        std::cout << "Error when building the triangle vertex shader module" << std::endl;
    else
        std::cout << "Triangle vertex shader successfully loaded" << std::endl;

    VkShaderModule triangleFragShader;
    if (!load_shader_module("../shaders/triangle.frag.spv", &triangleFragShader))
        std::cout << "Error when building the triangle fragment shader module" << std::endl;
    else
        std::cout << "Triangle fragment shader successfully loaded" << std::endl;

    // --- Pipeline Layout ---
    // Build the pipeline layout that controls the inputs/outputs of the shader.
    // We are not using descriptor sets or other systems yet, so no need to use anything other than empty default.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout));

    // --- Pipeline ---
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));
    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    // Vertex input controls how to read vertices from vertex buffers. We aren't using it yet.
    pipelineBuilder.vertexInputInfo = vkinit::vertex_input_state_create_info();

    // Input assembly is the configuration for drawing triangle lists, strips, or individual points. We are just going
    // to draw triangle list.
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

    // Use the triangle layout we created.
    pipelineBuilder.pipelineLayout = trianglePipelineLayout;

    trianglePipeline = pipelineBuilder.build_pipeline(device, renderPass);
}


void VulkanEngine::cleanup() {
    // SDL is a C library--it does not support constructors and destructors. We have to delete things manually.
    // Note: `VkPhysicalDevice` and `VkQueue` cannot be destroyed (they're something akin to handles).
    if (isInitialized) {
        // It is imperative that objects are destroyed in the opposite order that they are created (unless we really
        // know what we are doing)!
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        // Destroy the main renderpass
        vkDestroyRenderPass(device, renderPass, nullptr);

        // Destroy swapchain resources
        for (int i = 0; i < framebuffers.size(); i++) {
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
    }
}


void VulkanEngine::draw() {
    // --- Setup ---
    // Wait until the GPU has finished rendering the last frame (timeout = 1s)
    VK_CHECK(vkWaitForFences(device, 1, &renderFence, true, 1E9));
    VK_CHECK(vkResetFences(device, 1, &renderFence));

    // Request image from the swapchain (timeout = 1s). We use `presentSemaphore` to make sure that we can sync other
    // operations with the swapchain having an image ready to render.
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1E9, presentSemaphore, nullptr, &swapchainImageIndex));

    // Commands have finished execution at this point; we can safely reset the command buffer to begin enqueuing again.
    VK_CHECK(vkResetCommandBuffer(mainCommandBuffer, 0));

    VkCommandBuffer commandBuffer = mainCommandBuffer;
    // Begin the command buffer recording. As we will use this command buffer exactly once, we want to inform Vulkan
    // to allow for great optimization by the driver.
    VkCommandBufferBeginInfo commandBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr, // Used for secondary command buffers
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBeginInfo));

    // --- Main Renderpass ---
    // Make a clear-color from frame number. This will flash with a 120*π frame period.
    VkClearValue clearValue;
    float flash = abs(sin(static_cast<float>(frameNumber) / 120.0f));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    // Start the main renderpass. We will use the clear color from above, and the framebuffer of the index the swapchain
    // gave us.
    VkRenderPassBeginInfo renderpassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderPass,
            .framebuffer = framebuffers[swapchainImageIndex],
            .clearValueCount = 1,
            .pClearValues = &clearValue,
    };
    renderpassInfo.renderArea.offset.x = 0;
    renderpassInfo.renderArea.offset.y = 0;
    renderpassInfo.renderArea.extent = windowExtent;
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Once we start adding rendering commands, they will go here.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // Finalize the renderpass
    vkCmdEndRenderPass(commandBuffer);
    // Finalize the command buffer (we can no longer add commands, but the buffer is ready for execution).
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // Prepare the submission to the queue. We wait on `presentSemaphore`, as it's only signaled when the swapchain is
    // ready. We will then signal `renderSemaphore`, to inform that rendering has finished.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentSemaphore,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderSemaphore,
    };
    // Submit command buffer to the queue and execute it. `renderFence` will now block until the graphics commands
    // finish execution.
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, renderFence));

    // Put the rendered image into the visible window (after waiting on `renderSemaphore` as it's necessary that
    // drawing commands have finished before the image is displayed to the user).
    VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &swapchainImageIndex,
    };
    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameNumber++;
}


void VulkanEngine::run() {
    SDL_Event windowEvent;
    bool shouldQuit = false;

    // Main loop
    while (!shouldQuit) {
        // Handle all events the OS has sent to the application since the last frame.
        while (SDL_PollEvent(&windowEvent) != 0) {
            // Close the window when user presses alt-f4 or the 'x' button.
            switch(windowEvent.type) {
                case SDL_QUIT:
                    shouldQuit = true;
                    break;
                case SDL_KEYDOWN:
                    std::cout << "Key pressed!" << '\n';
                    break;
            }
        }
        draw();
    }
}