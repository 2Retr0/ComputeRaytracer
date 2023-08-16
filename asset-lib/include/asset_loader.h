#pragma once
#include <vector>
#include <string>

namespace assets {
    enum class CompressionMode : uint32_t {
        None,
        LZ4
    };

    /**
     * Struct for which textures and meshes are abstracted upon. Holds the entire compressed binary blob, so avoid
     * storing instances anywhere to avoid bloating up RAM usage.
     */
    struct AssetFile {
        char type[4]; // Textures=TEXI, Meshes=MESH
        int version;  // To ensure erroneous usage of older formats can be handled
        std::string json;
        std::vector<char> binaryBlob;
    };

    bool save_binaryfile(const char *path, const AssetFile &file);

    bool load_binaryfile(const char *path, AssetFile &outputFile);

    assets::CompressionMode parse_compression(const char* file);
} // namespace assets