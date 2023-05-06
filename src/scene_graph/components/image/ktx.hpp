#pragma once

#include "scene_graph/components/image.hpp"

namespace W3D::SceneGraph {
class Ktx : public Image {
   public:
    Ktx(const std::string& name, const std::vector<uint8_t>& data);
    virtual ~Ktx() = default;
};
}  // namespace W3D::SceneGraph