#include "vk_textures.h"
#include "asset_loader.h"
#include "texture_asset.h"
#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

AllocatedImage vkutil::load_image_from_file(VulkanEngine &engine, const std::string &path) {
    // --- Load File ---
    int width, height, channels;
    auto *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) throw std::runtime_error("ERROR: Failed to load texture file " + path);

    void *pixelPtr = pixels;
    auto imageSize = width * height * 4;          // 4 Bytes per pixel
    auto imageFormat = vk::Format::eR8G8B8A8Unorm; // The format R8G8B8A8 matches the pixel format loaded from stb_image.

    // --- Image Buffer Initialization ---
    // Allocate a temporary buffer for holding texture data for upload.
    auto stagingBuffer = engine.create_buffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);

    // Copy the data to the buffer
    void *textureData;
    textureData = engine.allocator->mapMemory(stagingBuffer.allocation);
    memcpy(textureData, pixelPtr, static_cast<size_t>(imageSize));
    engine.allocator->unmapMemory(stagingBuffer.allocation);

    // Free the loaded data (pixels) as they have been copied into the buffer.
    stbi_image_free(pixels);

    auto outImage = upload_image(width, height, imageFormat, engine, stagingBuffer);

    engine.allocator->destroyBuffer(stagingBuffer.buffer, stagingBuffer.allocation);

    std::cout << "Successfully loaded texture file " << path << std::endl;
    return outImage;
}


AllocatedImage vkutil::load_image_from_asset(VulkanEngine &engine, const std::string &path) {
    // --- Load File ---
    assets::AssetFile file;
    bool loaded = assets::load_binaryfile(path.c_str(), file);

    if (!loaded) throw std::runtime_error("ERROR: Failed to load texture file " + path);

    auto textureInfo = assets::read_texture_info(&file);
    auto imageSize = textureInfo.textureSize;
    vk::Format imageFormat;
    switch (textureInfo.textureFormat) {
        case assets::TextureFormat::RGBA8:
            imageFormat = vk::Format::eR8G8B8A8Unorm;
            break;
        default:
            throw std::runtime_error("ERROR: Failed to load texture file " + path);
    }

    // --- Image Buffer Initialization ---
    // Allocate a temporary buffer for holding texture data for upload.
    auto stagingBuffer = engine.create_buffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);

    // Copy the data to the buffer
    void *textureData;
    textureData = engine.allocator->mapMemory(stagingBuffer.allocation);
    assets::unpack_texture(&textureInfo, file.binaryBlob.data(), file.binaryBlob.size(), (char *) textureData);
    engine.allocator->unmapMemory(stagingBuffer.allocation);

    auto outImage = upload_image(textureInfo.pixelSize[0], textureInfo.pixelSize[1], imageFormat, engine, stagingBuffer);

    engine.allocator->destroyBuffer(stagingBuffer.buffer, stagingBuffer.allocation);

    std::cout << "Successfully loaded texture file " << path << std::endl;
    return outImage;
}


AllocatedImage vkutil::upload_image(int width, int height, vk::Format imageFormat, VulkanEngine &engine, AllocatedBuffer &stagingBuffer) {
    // --- Image Creation ---
    // Similar format to creating depth images, except we use the sampled and transfer destination usage flags since
    // the image will be used as a texture in shaders.
    auto imageExtent = vk::Extent3D(width, height, 1);
    auto imageInfo = vkinit::image_create_info(imageFormat, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, imageExtent);

    auto imageAllocInfo = vma::AllocationCreateInfo().setUsage(vma::MemoryUsage::eGpuOnly);
    // Allocate and create the image
    auto newImage = static_cast<AllocatedImage>(engine.allocator->createImage(imageInfo, imageAllocInfo));

    engine.immediate_submit([&](vk::CommandBuffer commandBuffer) {
        // clang-format off
        // --- Undefined->Transfer Layout Transition ---
        // Right now, the image is not initialized in any specific layout, so we need to do a layout transition so that the
        // driver puts the texture into Linear layout, which is the best for copying data from a buffer into a texture.
        auto range = vk::ImageSubresourceRange()
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)  // No mipmaps
            .setBaseArrayLayer(0)
            .setLayerCount(1); // No layered textures

        // To perform layout transitions, we need to use pipeline barriers. Pipeline barriers can control how the GPU
        // overlaps commands before and after the barrier. Using pipeline barriers with image barriers, the driver can
        // also transform the image to the correct formats and layouts.
        auto imageBarrierToTransfer = vk::ImageMemoryBarrier()
            .setSrcAccessMask({})
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setImage(newImage.image)
            .setSubresourceRange(range);
        // clang-format on

        // Barrier the image into the transfer-receive layout (barrier blocks from `TOP_OF_PIPE` to `TRANSFER` stages).
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                                      {}, nullptr, nullptr, {imageBarrierToTransfer});

        // --- Copy Staging Buffer to Image ---
        // Struct containing information of what to copy (buffer offset=0, mipmap level=0, layer=1)
        auto copyRegion = vk::BufferImageCopy(0, 0, 0).setImageExtent(imageExtent);
        copyRegion.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));

        // Copy the buffer into the image with specified optimal layout
        commandBuffer.copyBufferToImage(stagingBuffer.buffer, newImage.image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

        // --- Transfer->Shader Readable Layout Transition ---
        auto imageBarrierToReadable = imageBarrierToTransfer;
        imageBarrierToReadable.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        imageBarrierToReadable.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        imageBarrierToReadable.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        imageBarrierToReadable.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        // Barrier the image into the shader-readable layout.
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                                      {}, nullptr, nullptr, {imageBarrierToReadable});
    });

    // --- Cleanup ---
    engine.mainDeletionQueue.push([=, &engine]() {
        engine.allocator->destroyImage(newImage.image, newImage.allocation);
    });

    return newImage;
}
