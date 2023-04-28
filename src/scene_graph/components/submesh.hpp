#pragma once

#include "core/memory.hpp"
#include "scene_graph/component.hpp"

namespace W3D::SceneGraph {
class Material;
class SubMesh : public Component {
   public:
    SubMesh(const std::string &name = {});

    virtual ~SubMesh() = default;
    virtual std::type_index get_type() override;

    void set_material(const Material &material);
    const Material *get_material() const;

    std::uint32_t index_offset_ = 0;
    std::uint32_t vertices_count_ = 0;
    std::uint32_t vertex_indices_ = 0;

    std::unique_ptr<DeviceMemory::Buffer> pVertex_buffer_;
    std::unique_ptr<DeviceMemory::Buffer> pIndex_buffer_;

   private:
    const Material *pMaterial_{nullptr};
};
}  // namespace W3D::SceneGraph