#include "vk_textures.h"
#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

bool vkutil::load_image_from_file(VulkanEngine &engine, const char *path, AllocatedImage &outputImage) {
    // --- Load File ---
    int width, height, channels;
    auto *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        std::cout << "ERROR: Failed to load texture file " << path << std::endl;
        return false;
    }

    void *pixelPtr = pixels;
    auto imageSize = width * height * 4;        // 4 Bytes per pixel
    auto imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // The format R8G8B8A8 matches the pixel format loaded from stb_image.

    // --- Image Buffer Initialization ---
    // Allocate a temporary buffer for holding texture data for upload.
    auto stagingBuffer = engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    // Copy the data to the buffer
    void *textureData;
    vmaMapMemory(engine.allocator, stagingBuffer.allocation, &textureData);
    memcpy(textureData, pixelPtr, static_cast<size_t>(imageSize));
    vmaUnmapMemory(engine.allocator, stagingBuffer.allocation);

    // Free the loaded data (pixels) as they have been copied into the buffer.
    stbi_image_free(pixels);

    // --- Image Creation ---
    // Similar format to creating depth images, except we use the sampled and transfer destination usage flags since
    // the image will be used as a texture in shaders.
    VkExtent3D imageExtent = {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth = 1,
    };
    auto depthImageInfo = vkinit::image_create_info(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    AllocatedImage newImage = {};
    VmaAllocationCreateInfo depthImageAllocInfo = {.usage = VMA_MEMORY_USAGE_GPU_ONLY};

    // Allocate and create the image
    vmaCreateImage(engine.allocator, &depthImageInfo, &depthImageAllocInfo, &newImage.image, &newImage.allocation, nullptr);

    engine.immediate_submit([&](VkCommandBuffer commandBuffer) {
        // --- Undefined->Transfer Layout Transition ---
        // Right now, the image is not initialized in any specific layout, so we need to do a layout transition so that the
        // driver puts the texture into Linear layout, which is the best for copying data from a buffer into a texture.
        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1, // No mipmaps
            .baseArrayLayer = 0,
            .layerCount = 1, // No layered textures
        };

        // To perform layout transitions, we need to use pipeline barriers. Pipeline barriers can control how the GPU
        // overlaps commands before and after the barrier. Using pipeline barriers with image barriers, the driver can
        // also transform the image to the correct formats and layouts.
        VkImageMemoryBarrier imageBarrierToTransfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = newImage.image,
            .subresourceRange = range,
        };

        // Barrier the image into the transfer-receive layout (barrier blocks from `TOP_OF_PIPE` to `TRANSFER` stages).
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &imageBarrierToTransfer);

        // --- Copy Staging Buffer to Image ---
        // Struct containing information of what to copy (buffer offset=0, mipmap level=0, layer=1)
        VkBufferImageCopy copyRegion = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageExtent = imageExtent,
        };

        copyRegion.imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        // Copy the buffer into the image with specified optimal layout
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // --- Transfer->Shader Readable Layout Transition ---
        VkImageMemoryBarrier imageBarrierToReadable = imageBarrierToTransfer;
        imageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrierToReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrierToReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrierToReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Barrier the image into the shader-readable layout.
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &imageBarrierToReadable);
    });

    // --- Cleanup ---
    engine.mainDeletionQueue.push([=]() {
        vmaDestroyImage(engine.allocator, newImage.image, newImage.allocation);
    });
    vmaDestroyBuffer(engine.allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    std::cout << "Successfully loaded texture " << path << std::endl;
    outputImage = newImage;
    return true;
}
