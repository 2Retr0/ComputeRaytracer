#pragma once

#include "vk_types.h"
#include "vk_engine.h"

namespace vkutil {
    AllocatedImage load_image_from_file(VulkanEngine& engine, const std::string &path);
}
