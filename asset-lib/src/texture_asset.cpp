#include "texture_asset.h"

#include <lz4.h>
#include <nlohmann/json.hpp>

static assets::TextureFormat parse_format(const char *file) {
    if (strcmp(file, "RGBA8") == 0)
        return assets::TextureFormat::RGBA8;
    return assets::TextureFormat::Unknown;
}

assets::AssetFile assets::pack_texture(assets::TextureInfo *info, void *pixelData) {
    AssetFile file;
    file.type[0] = 'T';
    file.type[1] = 'E';
    file.type[2] = 'X';
    file.type[3] = 'I';
    file.version = 1;

    nlohmann::json textureMetadata;
    textureMetadata["format"] = "RGBA8";
    textureMetadata["width"] = info->pixelSize[0];
    textureMetadata["height"] = info->pixelSize[1];
    textureMetadata["buffer_size"] = info->textureSize;
    textureMetadata["original_file"] = info->originalFile;
    textureMetadata["compression"] = "LZ4";

    // Compress buffer into a blob.
    // Find the maximum data needed for the compression
    int compressStaging = LZ4_compressBound(info->textureSize);
    // Make sure the blob storage has enough size for the maximum
    file.binaryBlob.resize(compressStaging);

    // This is like `memcpy()`, except it compresses the data and returns the compressed size
    int compressedSize = LZ4_compress_default((const char *) pixelData, file.binaryBlob.data(), info->textureSize, compressStaging);
    // We can now resize the blob down to the final compressed size.
    file.binaryBlob.resize(compressedSize);

    auto stringified = textureMetadata.dump();
    file.json = stringified;

    return file;
}


assets::TextureInfo assets::read_texture_info(AssetFile *file) {
    TextureInfo info;
    auto textureMetadata = nlohmann::json::parse(file->json);

    std::string format = textureMetadata["format"];
    info.textureFormat = parse_format(format.c_str());

    std::string compression = textureMetadata["compression"];
    info.compressionMode = parse_compression(compression.c_str());

    info.pixelSize[0] = textureMetadata["width"];
    info.pixelSize[1] = textureMetadata["height"];
    info.textureSize = textureMetadata["buffer_size"];
    info.originalFile = textureMetadata["original_file"];

    return info;
}


void assets::unpack_texture(TextureInfo *info, const char *sourceBuffer, int sourceSize, char *destination) {
    if (info->compressionMode == CompressionMode::LZ4)
        LZ4_decompress_safe(sourceBuffer, destination, sourceSize, info->textureSize);
    else
        memcpy(destination, sourceBuffer, sourceSize);
}