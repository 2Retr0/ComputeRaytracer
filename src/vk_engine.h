#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

// vulkan_guide.h : Include file for standard system include files, or project specific include files.
class VulkanEngine {
public:
    bool is_initialized{false};
    int frame_number{0};
    VkExtent2D window_extent{1700, 900};
    struct SDL_Window *window{nullptr};

    // Initializes everything in the engine.
    void init();

    // Shuts down the engine.
    void cleanup();

    // Draw loop.
    void draw();

    // Run main loop.
    void run();
};

void VulkanEngine::init() {
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    auto window_flags = (SDL_WindowFlags) (SDL_WINDOW_VULKAN);

    window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(window_extent.width),
        static_cast<int>(window_extent.height),
        window_flags
    );

    // Everything went fine!
    is_initialized = true;
}

void VulkanEngine::cleanup() {
    if (is_initialized) {
        SDL_DestroyWindow(window);
    }
}

void VulkanEngine::draw() {
    // Nothing yet
}

void VulkanEngine::run() {
    SDL_Event window_event;
    bool should_quit = false;

    // Main loop
    while (!should_quit) {
        // Handle events on queue
        while (SDL_PollEvent(&window_event) != 0) {
            // Close the window when user alt-F4s or clicks the X button
            if (window_event.type == SDL_QUIT) should_quit = true;
        }
        draw();
    }
}
