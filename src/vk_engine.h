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
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    // --- Structure Synchronization ---
    VkSemaphore presentSemaphore, renderSemaphore;
    VkFence renderFence;

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();
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
