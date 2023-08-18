#pragma once

#include "vk_engine.h"
#include "vk_types.h"

namespace vkutil {
    AllocatedImage load_image_from_file(VulkanEngine &engine, const std::string &path);
    AllocatedImage load_image_from_asset(VulkanEngine &engine, const std::string &path);
    AllocatedImage upload_image(int width, int height, vk::Format imageFormat, VulkanEngine &engine, AllocatedBuffer &stagingBuffer);
} // namespace vkutil
