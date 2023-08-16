#pragma once
#include "asset_loader.h"

namespace assets {
    enum class TextureFormat : uint32_t {
        Unknown = 0,
        RGBA8
    };

    struct TextureInfo {
        int textureSize;
        TextureFormat textureFormat;
        CompressionMode compressionMode;
        uint32_t pixelSize[3];
        std::string originalFile;
    };

    /** Parses the texture metadata from an asset file. */
    TextureInfo read_texture_info(AssetFile *file);

    /** Decompresses a texture into a destination buffer based on its info alongside a binary blob of pixel data. */
    void unpack_texture(TextureInfo *info, const char *sourceBuffer, int sourceSize, char *destinationBuffer);

    AssetFile pack_texture(TextureInfo *info, void *pixelData);
} // namespace assets