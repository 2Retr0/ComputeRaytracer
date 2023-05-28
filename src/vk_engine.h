#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>
#include <vk_mesh.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "VkBootstrap.h"
#include "VkBootstrapDispatch.h"

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
    VkPipelineVertexInputStateCreateInfo         vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo       inputAssembly{};
    VkViewport                                   viewport{};
    VkRect2D                                     scissor{};
    VkPipelineRasterizationStateCreateInfo       rasterizer{};
    VkPipelineColorBlendAttachmentState          colorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo         multisampling{};
    VkPipelineLayout                             pipelineLayout{};
    VkPipelineDepthStencilStateCreateInfo        depthStencil{};

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
            .pDepthStencilState = &depthStencil,
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


struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
};


class VulkanEngine {
public:
    // Initializes everything in the engine.
    void init();

    // Shuts down the engine.
    void cleanup();

    // Draw loop.
    void draw();

    // Run the main loop.
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
    VkRenderPass               renderpass;
    std::vector<VkFramebuffer> framebuffers;
    VkImageView                depthImageView;
    AllocatedImage             depthImage;
    VkFormat                   depthFormat;    // The format for the depth image.

    // --- Structure Synchronization ---
    VkSemaphore presentSemaphore, renderSemaphore;
    VkFence     renderFence;

    // --- Pipeline ---
    int              selectedShader {0};
    VkPipelineLayout trianglePipelineLayout;
    VkPipeline       trianglePipeline;
    VkPipeline       redTrianglePipeline;
    VkPipelineLayout meshPipelineLayout;
    VkPipeline       meshPipeline;
    DeletionQueue    mainDeletionQueue;

    // --- Memory ---
    VmaAllocator allocator;
    Mesh         triangleMesh;
    Mesh         monkeyMesh;

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    // Loads a shader module from a spir-v file. Returns false if it errors
    bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule) const;

    void load_meshes();

    void upload_mesh(Mesh& mesh);

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
    load_meshes();

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

    // --- Initialize Memory Allocator ---
    VmaAllocatorCreateInfo allocatorInfo = {
            .physicalDevice = chosenGPU,
            .device = device,
            .instance = instance,
    };
    vmaCreateAllocator(&allocatorInfo, &allocator);
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

    mainDeletionQueue.push([this]() { vkDestroySwapchainKHR(device, swapchain, nullptr); });

    // --- Depth Image ---
    VkExtent3D depthImageExtent = { windowExtent.width, windowExtent.height, 1 }; // Depth image size matches window

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
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    // Allocate the other default command buffer that we will use for rendering.
    auto commandAllocateInfo = vkinit::command_buffer_allocate_info(commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &commandAllocateInfo, &mainCommandBuffer));

    mainDeletionQueue.push([this]() { vkDestroyCommandPool(device, commandPool, nullptr); });
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
    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
    VkSubpassDependency dependencies[2] = { colorDependency, depthDependency };
    // The Vulkan driver will perform the layout transitions for us when using the renderpass. If we weren't using a
    // renderpass (drawing from compute shaders) we would need to do the same transitions explicitly.
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
    VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .attachmentCount = 1,
            .width = windowExtent.width,
            .height = windowExtent.height,
            .layers = 1,
    };

    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    // Create framebuffers for each of the swapchain image views. We connect the depth image view when creating each of
    // the framebuffers. We do not need to change the depth image between frames and can just clear and reuse the same
    // depth image for every.
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[2] = { swapchainImageViews[i], depthImageView };
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
    // --- Create Fence ---
    // We want to create the fence with the `CREATE_SIGNALED` flag, so we can wait on it before using it on a
    // GPU command (for the first frame)
    auto fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));

    // Enqueue the destruction of the fence
    mainDeletionQueue.push([this]() { vkDestroyFence(device, renderFence, nullptr); });

    // --- Create Semaphores ---
    auto semaphoreCreateInfo = vkinit::semaphore_create_info();
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSemaphore));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore));

    // Enqueue the destruction of semaphores
    mainDeletionQueue.push([this]() {
        vkDestroySemaphore(device, presentSemaphore, nullptr);
        vkDestroySemaphore(device, renderSemaphore, nullptr);
    });
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


void VulkanEngine::load_meshes() {
    triangleMesh.vertices.resize(3);

    triangleMesh.vertices[0].position = { 1.f, 1.f, 0.f };
    triangleMesh.vertices[1].position = {-1.f, 1.f, 0.f };
    triangleMesh.vertices[2].position = { 0.f,-1.f, 0.f };

    triangleMesh.vertices[0].color = { 0.f, 1.f, 0.f };
    triangleMesh.vertices[1].color = { 0.f, 1.f, 0.f };
    triangleMesh.vertices[2].color = { 0.f, 1.f, 0.f };

    monkeyMesh.load_from_obj("../assets/monkey_smooth.obj");

    // We need to make sure both meshes are sent to the GPU. We don't care about vertex normals.
    upload_mesh(triangleMesh);
    upload_mesh(monkeyMesh);
}


void VulkanEngine::upload_mesh(Mesh& mesh) {
    // Allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = mesh.vertices.size() * sizeof(Vertex), // The total size, in bytes, of the buffer to allocate
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // Buffer is to be used as vertex buffer
    };

    // Let the VMA library know that this data should be writable only by CPU, but readable by GPU
    VmaAllocationCreateInfo vmaAllocInfo = { .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };

    // Allocate the buffer
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    // Add the destruction of the triangle mesh buffer to the deletion queue
    mainDeletionQueue.push([=, this]() {
        vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
    });

    // To push data into a VkBuffer, we need to map it first. Mapping a buffer will give us a pointer and, once we are
    // done with writing the data, we can unmap.
    void* vertexData;
    vmaMapMemory(allocator, mesh.vertexBuffer.allocation, &vertexData);
    memcpy(vertexData, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, mesh.vertexBuffer.allocation);
}


void VulkanEngine::init_pipelines() {
    // --- Pipeline Layout ---
    // Build the pipeline layout that controls the inputs/outputs of the shader.
    // We are not using descriptor sets or other systems yet, so no need to use anything other than empty default.
    auto trianglePipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(device, &trianglePipelineLayoutInfo, nullptr, &trianglePipelineLayout));

    auto meshPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    // Setup push constants
    VkPushConstantRange pushConstant = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // Push constant range is accessible only in the vertex shader
            .offset = 0,                              // Push constant range starts at the beginning
            .size = sizeof(MeshPushConstants),        // Push constant range has size of `MeshPushConstants` struct
    };

    meshPipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    meshPipelineLayoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(device, &meshPipelineLayoutInfo, nullptr, &meshPipelineLayout));

    // --- Pipeline ---
    PipelineBuilder pipelineBuilder;
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
    // Default depth testing
    pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);


    // --- Red Triangle Shader Module ---
    VkShaderModule redTriangleVertShader;
    if (!load_shader_module("../shaders/triangle.vert.spv", &redTriangleVertShader))
        std::cout << "Error when building the red triangle vertex shader module" << std::endl;
    else
        std::cout << "Red Triangle vertex shader successfully loaded" << std::endl;

    VkShaderModule redTriangleFragShader;
    if (!load_shader_module("../shaders/triangle.frag.spv", &redTriangleFragShader))
        std::cout << "Error when building the red triangle fragment shader module" << std::endl;
    else
        std::cout << "Red Triangle fragment shader successfully loaded" << std::endl;

    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVertShader));
    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFragShader));

    redTrianglePipeline = pipelineBuilder.build_pipeline(device, renderpass);

    // --- Triangle Shader Module ---
    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder

    VkShaderModule triangleVertexShader;
    if (!load_shader_module("../shaders/colored_triangle.vert.spv", &triangleVertexShader))
        std::cout << "Error when building the triangle vertex shader module" << std::endl;
    else
        std::cout << "Triangle vertex shader successfully loaded" << std::endl;

    VkShaderModule triangleFragShader;
    if (!load_shader_module("../shaders/colored_triangle.frag.spv", &triangleFragShader))
        std::cout << "Error when building the triangle fragment shader module" << std::endl;
    else
        std::cout << "Triangle fragment shader successfully loaded" << std::endl;

    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));
    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    trianglePipeline = pipelineBuilder.build_pipeline(device, renderpass);


    // --- Mesh Pipeline ---
    auto vertexDescription = Vertex::get_vertex_description();

    // Connect the pipeline builder vertex input info to the one we get from Vertex
    pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

    pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    // Now our mesh pipeline has the space for the push constants, so we can now execute the command to use them.
    pipelineBuilder.pipelineLayout = meshPipelineLayout;

    // --- Triangle Mesh Shader Module ---
    pipelineBuilder.shaderStages.clear(); // Clear the shader stages for the builder

    VkShaderModule meshVertShader;
    if (!load_shader_module("../shaders/tri_mesh.vert.spv", &meshVertShader))
        std::cout << "Error when building the triangle mesh vertex module" << std::endl;
    else
        std::cout << "Triangle mesh vertex shader successfully loaded" << std::endl;

    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
    pipelineBuilder.shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    meshPipeline = pipelineBuilder.build_pipeline(device, renderpass);

    // --- Cleanup ---
    // Destroy all shader modules, outside the queue
    vkDestroyShaderModule(device, redTriangleVertShader, nullptr);
    vkDestroyShaderModule(device, redTriangleFragShader, nullptr);
    vkDestroyShaderModule(device, triangleFragShader, nullptr);
    vkDestroyShaderModule(device, triangleVertexShader, nullptr);
    vkDestroyShaderModule(device, meshVertShader, nullptr);

    mainDeletionQueue.push([this]() {
        // Destroy the 2 pipelines we have created
        vkDestroyPipeline(device, redTrianglePipeline, nullptr);
        vkDestroyPipeline(device, trianglePipeline, nullptr);
        vkDestroyPipeline(device, meshPipeline, nullptr);

        // Destroy the pipeline layout that they use
        vkDestroyPipelineLayout(device, trianglePipelineLayout, nullptr);
        vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
    });
}


void VulkanEngine::cleanup() {
    // SDL is a C library--it does not support constructors and destructors. We have to delete things manually.
    // Note: `VkPhysicalDevice` and `VkQueue` cannot be destroyed (they're something akin to handles).
    if (isInitialized) {
        // It is imperative that objects are destroyed in the opposite order that they are created (unless we really
        // know what we are doing)!
        vkWaitForFences(device, 1, &renderFence, true, 1E9);

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

    auto commandBuffer = mainCommandBuffer;
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

    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.0f; // Clear depth at z = 1

    // Start the main renderpass. We will use the clear color from above, and the framebuffer of the index the swapchain
    // gave us.
    VkClearValue clearValues[] = { clearValue, depthClear };
    VkRenderPassBeginInfo renderpassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .framebuffer = framebuffers[swapchainImageIndex],
            .clearValueCount = 2,
            .pClearValues = &clearValues[0],
    };
    renderpassInfo.renderArea.offset.x = 0;
    renderpassInfo.renderArea.offset.y = 0;
    renderpassInfo.renderArea.extent = windowExtent;
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    auto setupViewMatrix = [=, this]() {
        // --- Object Model-View Matrix ---
        // Camera position setup
        glm::vec3 cameraPosition = { 0.f, 0.f,-2.f };
        auto view = glm::translate(glm::mat4(1.f), cameraPosition);

        // Camera projection setup
        auto projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.f);
        projection[1][1] *= -1;

        // Model rotation setup
        auto model = glm::rotate(glm::mat4(1.f), glm::radians(static_cast<float>(frameNumber) * 0.4f), glm::vec3(0, 1, 0));

        // Calculate final mesh matrix
        auto meshMatrix = projection * view * model;

        // Upload the matrix to the GPU via push constants
        MeshPushConstants constants = { .renderMatrix = meshMatrix };
        return constants;
    };

    // Once we start adding rendering commands, they will go here.
    VkDeviceSize offset = 0; // Bind mesh vertex buffers with offset 0
    MeshPushConstants constants = setupViewMatrix();
    switch(selectedShader) {
        case 0:
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, redTrianglePipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            break;
        case 1:
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            break;
        case 2:
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &triangleMesh.vertexBuffer.buffer, &offset);
            vkCmdPushConstants(commandBuffer, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
            vkCmdDraw(commandBuffer, triangleMesh.vertices.size(), 1, 0, 0);
            break;
        case 3:
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &monkeyMesh.vertexBuffer.buffer, &offset);
            vkCmdPushConstants(commandBuffer, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
            vkCmdDraw(commandBuffer, monkeyMesh.vertices.size(), 1, 0, 0);
            break;
    }

    vkCmdEndRenderPass(commandBuffer);           // Finalize the renderpass
    VK_CHECK(vkEndCommandBuffer(commandBuffer)); // Finalize the command buffer for execution--we can no longer add commands.

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
            if (windowEvent.type == SDL_QUIT) {
                shouldQuit = true;
            } else if (windowEvent.type == SDL_KEYDOWN) {
                switch(windowEvent.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (++selectedShader > 3) selectedShader = 0;
                        break;
                }
            }
        }
        draw();
    }
}