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
    bool               isInitialized{false};
    int                frameNumber{0};
    VkExtent2D         windowExtent{1280, 800}; // The width and height of the window (px)
    struct SDL_Window *window{nullptr};         // Forward-declaration for the window

    VkInstance               instance;       // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger; // Vulkan debug output handle
    VkPhysicalDevice         chosenGPU;      // GPU chosen as the default device
    VkDevice                 device;         // Vulkan device for commands
    VkSurfaceKHR             surface;        // Vulkan window surface

    VkSwapchainKHR           swapchain;
    VkFormat                 swapchainImageFormat; // Image format expected by the windowing system
    std::vector<VkImage>     swapchainImages;      // Array of images from the swapchain
    std::vector<VkImageView> swapchainImageViews;  // Array of image-views from the swapchain

    VkQueue         graphicsQueue;       // The queue we will submit to.
    uint32_t        graphicsQueueFamily; // The family of said queue.
    VkCommandPool   commandPool;         // The command pool for our commands.
    VkCommandBuffer mainCommandBuffer;   // The buffer we will record into.

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();
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

    init_vulkan();    // Load the core Vulkan structures
    init_swapchain(); // Create the swapchain
    init_commands();

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


void VulkanEngine::cleanup() {
    // SDL is a C library--it does not support constructors and destructors. We have to delete things manually.
    // Note: `VkPhysicalDevice` and `VkQueue` cannot be destroyed (they're something akin to handles).
    if (isInitialized) {
        // It is imperative that objects are destroyed in the opposite order that they are created (unless we really
        // know what we are doing)!
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        // Destroy swapchain resources
        for (auto& imageView : swapchainImageViews)
            vkDestroyImageView(device, imageView, nullptr);

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
    }
}


void VulkanEngine::draw() {
    // Nothing yet
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
