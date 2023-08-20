#pragma once

#include "vk_mesh.h"
#include "vk_types.h"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <deque>
#include <functional>
#include <ranges>
#include <string>
#include <unordered_map>

// Number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push(std::function<void()> &&function) {
        deletors.push_back(function);
    }

    void flush() {
        // Reverse iterate the deletion queue to execute all the functions
        for (auto &deletor: std::ranges::reverse_view(deletors))
            deletor();
        deletors.clear();
    }
};


struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 renderMatrix;
};


struct Material {
    vk::raii::DescriptorSet textureSet = nullptr;
    // Note: We store `vk::Pipeline` and layout by value, not pointer. They are 64-bit handles to internal driver
    //       structures anyway, so storing pointers to them isn't very useful.
    vk::raii::Pipeline pipeline = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
};


/**
 * Holds the data needed for a single draw. We will hold objects in an array to render in-order.
 */
struct RenderObject {
    Mesh *mesh;
    Material *material;
    glm::mat4 transformMatrix;
};


struct GPUCameraData {
    glm::mat4 view;           // Camera position
    glm::mat4 projection;     // Perspective transform
    glm::mat4 viewProjection; // View and proj matrices multiplied together (to avoid multiplying in shader).
};


struct GPUSceneData {
    glm::vec4 fogColor;          // w=exponent
    glm::vec4 fogDistances;      // x=min; y=max; z,w=unused.
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w=power
    glm::vec4 sunlightColor;
};


struct GPUObjectData {
    glm::mat4 modelMatrix;
};


/** Includes rendering-related structures to control object lifetimes easier (e.g., double buffering). */
struct FrameData {
    // --- Structure Synchronization ---
    vk::raii::Semaphore presentSemaphore = nullptr;
    vk::raii::Semaphore renderSemaphore = nullptr;
    vk::raii::Fence renderFence  = nullptr;

    // --- Commands ---
    vk::raii::CommandPool commandPool = nullptr;         // The command pool for our commands.
    vk::raii::CommandBuffer mainCommandBuffer = nullptr; // The buffer we will record into.

    // --- Descriptor Sets ---
//    AllocatedBuffer cameraBuffer; // Buffer that holds a single `GPUCameraData` to use when rendering.
//    vk::DescriptorSet globalDescriptor;

    // --- Memory ---
    AllocatedBuffer objectBuffer;
    vk::raii::DescriptorSet objectDescriptor = nullptr;
};


/** Abstraction for short-lived commands (e.g., copying meshes from a CPU -> GPU buffer). Stores upload-related structs. */
struct UploadContext {
    vk::raii::Fence uploadFence = nullptr;
    vk::raii::CommandPool commandPool = nullptr;
    vk::raii::CommandBuffer commandBuffer = nullptr;
};


struct Texture {
    AllocatedImage image;
    vk::raii::ImageView imageView = nullptr;
};



class VulkanEngine {
public:
    /** Initializes everything in the engine. */
    void init();

    /** Shuts down the engine. */
    void cleanup();

    /** Draw loop. */
    void draw();

    /** Run the main loop. */
    void run();

    [[nodiscard]] AllocatedBuffer create_buffer(size_t size, vk::BufferUsageFlags flags, vma::MemoryUsage memoryUsage);

    void immediate_submit(std::function<void(vk::CommandBuffer commandBuffer)> &&function) const;

public:
    // --- Window ---
    bool isInitialized = false;
    int frameNumber = 0;
    int animationFrameNumber = 0;
    vk::Extent2D windowExtent = {1280, 800}; // The width and height of the window (px)
    struct SDL_Window *window;               // Forward-declaration for the window
    uint64_t ticksMs = 0;
    int fps = 0;

    // --- Vulkan ---
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;                      // Vulkan library handle
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;  // Vulkan debug output handle
    vk::raii::PhysicalDevice chosenGPU = nullptr;               // GPU chosen as the default device
    vk::raii::Device device = nullptr;                          // Vulkan device for commands
    vk::raii::SurfaceKHR surface = nullptr;                     // Vulkan window surface
    vk::PhysicalDeviceProperties gpuProperties;

    // --- Swapchain ---
    vk::raii::SwapchainKHR swapchain = nullptr;
    vk::Format swapchainImageFormat;                      // Image format expected by the windowing system
    std::vector<VkImage> swapchainImages;                 // Array of images from the swapchain
    std::vector<vk::raii::ImageView> swapchainImageViews; // Array of image-views from the swapchain

    // --- Commands ---
    vk::Queue graphicsQueue;      // The queue we will submit to.
    uint32_t graphicsQueueFamily; // The family of said queue.

    // --- Renderpass ---
    vk::raii::RenderPass renderpass = nullptr;
    std::vector<vk::raii::Framebuffer> framebuffers;
    vk::raii::ImageView depthImageView = nullptr;
    AllocatedImage depthImage;
    vk::Format depthFormat; // The format for the depth image.

    // --- Memory ---
    DeletionQueue mainDeletionQueue;
    vma::UniqueAllocator allocator;
    UploadContext uploadContext;

    // --- Scene Management ---
    std::vector<RenderObject> renderables;
    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;
    GPUSceneData sceneParameters;
    AllocatedBuffer sceneParameterBuffer;

    // --- Camera ---
    glm::vec3 camera = glm::vec3(0.f, 6.f, 10.f);
    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 forward = glm::vec3(0.f, 0.f, -1.f);

    // --- Double Buffering ---
    FrameData frames[FRAME_OVERLAP];

    // --- Descriptor Sets ---
    vk::raii::DescriptorSetLayout globalSetLayout = nullptr;
    vk::raii::DescriptorSetLayout objectSetLayout = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    vk::raii::DescriptorPool imguiPool = nullptr;
    vk::raii::DescriptorSet globalDescriptor = nullptr;

    // --- Textures ---
    std::unordered_map<std::string, Texture> loadedTextures;
    vk::raii::DescriptorSetLayout singleTextureSetLayout = nullptr;
    vk::raii::Sampler blockySampler = nullptr;

    // --- Compute ---
    vk::raii::DescriptorSetLayout computeSetLayout = nullptr;
    vk::raii::DescriptorSet computeDescriptor = nullptr;
    vk::raii::DescriptorSetLayout graphicsSetLayout = nullptr;
    vk::raii::DescriptorSet graphicsDescriptor = nullptr;
    Texture computeTexture;

private:
    void init_vulkan();

    void init_swapchain();

    void init_commands();

    void init_default_renderpass();

    void init_framebuffers();

    void init_sync_structures();

    void init_pipelines();

    /** Creates the `renderObjects` for a scene. */
    void init_scene();

    void init_descriptors();

    void init_imgui();

    /** Loads a shader module from a spir-v file. Returns false if it errors. */
    vk::raii::ShaderModule load_shader_module(const char *path) const;

    void load_images();

    void load_meshes();

    void upload_mesh(Mesh &mesh);

    void draw_objects(const vk::raii::CommandBuffer &commandBuffer, RenderObject *first, uint32_t count);

    /** Creates a material and adds it to a map. */
    void create_material(vk::raii::Pipeline pipeline, vk::raii::PipelineLayout layout, const std::string &name);

    size_t pad_uniform_buffer_size(size_t originalSize) const;

    /** Returns `nullptr` if the material cannot be found. */
    Material *get_material(const std::string &name);

    /** Returns `nullptr` if the mesh cannot be found. */
    Mesh *get_mesh(const std::string &name);

    FrameData &get_current_frame();
};