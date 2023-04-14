#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace W3D {
class ResourceManager {
   public:
    std::vector<char> readFile(const std::string& filename);
};
}  // namespace W3D