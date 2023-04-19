#include "resource_manager.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "model.hpp"
#include "tiny_gltf.h"

const std::string SHADER_FOLDER = "shaders/";
const std::string MODEL_FOLDER = "../data/models/";

namespace W3D {
std::vector<char> ResourceManager::loadShaderBinary(const std::string &filename) {
    std::string path = SHADER_FOLDER + filename;
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    };

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
};

tinygltf::Model ResourceManager::loadGLTFModel(const std::string &filename) {
    std::string extension = getFileExtension(filename);
    std::string path = MODEL_FOLDER + filename;

    std::string err, warning;
    tinygltf::Model raw;
    bool didLoadRawModel;
    if (extension == "glb") {
        didLoadRawModel = gltfContext_.LoadBinaryFromFile(&raw, &err, &warning, path);
    } else {
        didLoadRawModel = gltfContext_.LoadASCIIFromFile(&raw, &err, &warning, path);
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!warning.empty()) {
        std::cerr << warning << std::endl;
    }

    if (!didLoadRawModel) {
        throw std::runtime_error("Failed to load model");
    }

    return raw;
}

std::string ResourceManager::getFileExtension(const std::string &filename) {
    auto extensionPos = filename.find_last_of(".");
    if (extensionPos != std::string::npos) {
        return filename.substr(extensionPos, +1);
    }
    return "";
}

}  // namespace W3D