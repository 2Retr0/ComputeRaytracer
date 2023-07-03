#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "vk_mesh.h"
#include "vk_types.h"

#include <deque>
#include <functional>
#include <ranges>
#include <unordered_map>

// Number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;

class PipelineBuilder {
public:
    // This is a basic set of required Vulkan structs for pipeline creation, there are more, but for now these are the
    // ones we will need to fill for now.
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineLayout pipelineLayout{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    VkPipeline build_pipeline(VkDevice device, VkRenderPass renderpass);
};


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
    // Note: We store `VkPipeline` and layout by value, not pointer. They are 64-bit handles to internal driver
    //       structures anyway, so storing pointers to them isn't very useful.
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
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
    VkSemaphore presentSemaphore, renderSemaphore;
    VkFence renderFence;

    // --- Commands ---
    VkCommandPool commandPool;         // The command pool for our commands.
    VkCommandBuffer mainCommandBuffer; // The buffer we will record into.

    // --- Descriptor Sets ---
    AllocatedBuffer cameraBuffer; // Buffer that holds a single `GPUCameraData` to use when rendering.
    VkDescriptorSet globalDescriptor;

    // --- Memory ---
    AllocatedBuffer objectBuffer;
    VkDescriptorSet objectDescriptor;
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

public:
    // --- Window ---
    bool isInitialized = false;
    int frameNumber = 0;
    VkExtent2D windowExtent = {1280, 800}; // The width and height of the window (px)
    struct SDL_Window *window = nullptr;   // Forward-declaration for the window

    // --- Vulkan ---
    VkInstance instance;                      // Vulkan library handle
    VkDebugUtilsMessengerEXT debugMessenger;  // Vulkan debug output handle
    VkPhysicalDevice chosenGPU;               // GPU chosen as the default device
    VkDevice device;                          // Vulkan device for commands
    VkSurfaceKHR surface;                     // Vulkan window surface
    VkPhysicalDeviceProperties gpuProperties;

    // --- Swapchain ---
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;                // Image format expected by the windowing system
    std::vector<VkImage> swapchainImages;         // Array of images from the swapchain
    std::vector<VkImageView> swapchainImageViews; // Array of image-views from the swapchain

    // --- Commands ---
    VkQueue graphicsQueue;        // The queue we will submit to.
    uint32_t graphicsQueueFamily; // The family of said queue.

    // --- Renderpass ---
    VkRenderPass renderpass;
    std::vector<VkFramebuffer> framebuffers;
    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat; // The format for the depth image.

    // --- Memory ---
    DeletionQueue mainDeletionQueue;
    VmaAllocator allocator;

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
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout objectSetLayout;
    VkDescriptorPool descriptorPool;

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

    /** Loads a shader module from a spir-v file. Returns false if it errors. */
    bool load_shader_module(const char *filePath, VkShaderModule *outShaderModule) const;

    void load_meshes();

    void upload_mesh(Mesh &mesh);

    void draw_objects(VkCommandBuffer commandBuffer, RenderObject *first, uint32_t count);

    /** Creates a material and adds it to a map. */
    Material *create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name);

    AllocatedBuffer create_buffer(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage memoryUsage) const;

    size_t pad_uniform_buffer_size(size_t originalSize) const;

    /** Returns `nullptr` if the material cannot be found. */
    Material *get_material(const std::string &name);

    /** Returns `nullptr` if the mesh cannot be found. */
    Mesh *get_mesh(const std::string &name);

    FrameData &get_current_frame();
};