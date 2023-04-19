#pragma once

#include <memory>
#include <vector>

namespace W3D {
namespace gltf {
class Model;
}
class SceneGraph {
   private:
    std::vector<std::unique_ptr<gltf::Model>> models_;
};
}  // namespace W3D