#include "asset_loader.h"

#include <fstream>
#include <iostream>

assets::CompressionMode assets::parse_compression(const char* file) {
    if (strcmp(file, "LZ4") == 0)
        return assets::CompressionMode::LZ4;
    return assets::CompressionMode::None;
}


bool assets::save_binaryfile(const char *path, const assets::AssetFile &file) {
    std::ofstream outfile;
    outfile.open(path, std::ios::binary | std::ios::out);

    outfile.write(file.type, 4);

    uint32_t version = file.version;
    outfile.write((const char *) &version, sizeof(uint32_t));

    uint32_t length = file.json.size();
    outfile.write((const char *) &length, sizeof(uint32_t));

    uint32_t blobLength = file.binaryBlob.size();
    outfile.write((const char *) &blobLength, sizeof(uint32_t));

    outfile.write(file.json.data(), length);

    outfile.write(file.binaryBlob.data(), file.binaryBlob.size());

    outfile.close();
    return true;
}


bool assets::load_binaryfile(const char *path, assets::AssetFile &outputFile) {
    std::ifstream infile;
    infile.open(path, std::ios::binary);

    if (!infile.is_open()) return false;

    infile.seekg(0); // Move the file cursor to the beginning of the file

    infile.read(outputFile.type, 4);
    infile.read((char *) &outputFile.version, sizeof(uint32_t));

    uint32_t jsonLength = 0;
    infile.read((char *) &jsonLength, sizeof(uint32_t));

    uint32_t blobLength = 0;
    infile.read((char *) &blobLength, sizeof(uint32_t));

    outputFile.json.resize(jsonLength);
    infile.read(outputFile.json.data(), jsonLength);

    outputFile.binaryBlob.resize(blobLength);
    infile.read(outputFile.binaryBlob.data(), blobLength);

    return true;
}