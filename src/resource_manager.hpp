#pragma once
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "tiny_gltf.h"

namespace W3D {
namespace gltf {
class Model;
}
class ResourceManager {
   public:
    std::vector<char> loadShaderBinary(const std::string& filename);
    tinygltf::Model loadGLTFModel(const std::string& filename);

   private:
    std::string getFileExtension(const std::string& path);

    tinygltf::TinyGLTF gltfContext_;
};
}  // namespace W3D