#pragma once
#include "common/glm_common.hpp"
#include "scene_graph/component.hpp"
namespace W3D::SceneGraph {
class AABB : public Component {
   public:
    AABB();
    AABB(const glm::vec3& min, const glm::vec3& max);
    virtual ~AABB() = default;
    virtual std::type_index get_type() override;

    void update(const glm::vec3& pt);
    void transform(glm::mat4& T);
    glm::vec3 get_scale() const;
    glm::vec3 get_center() const;
    glm::vec3 get_min() const;
    glm::vec3 get_max() const;
    void reset();

   private:
    glm::vec3 min_;
    glm::vec3 max_;
};
}  // namespace W3D::SceneGraph