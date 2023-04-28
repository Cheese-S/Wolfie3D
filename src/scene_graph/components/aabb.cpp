#include "aabb.hpp"

namespace W3D::SceneGraph {
AABB::AABB() {
    reset();
}

AABB::AABB(const glm::vec3 &min, const glm::vec3 &max) : min_(min), max_(max) {
}

std::type_index AABB::get_type() {
    return typeid(AABB);
}

void AABB::update(const glm::vec3 &point) {
    min_ = glm::min(min_, point);
    max_ = glm::max(max_, point);
}
// AABB Transform algorithm by Jim Arvo
// See https://www.realtimerendering.com/resources/GraphicsGems/gems/TransBox.c
void AABB::transform(glm::mat4 &T) {
    float a, b;
    glm::vec3 new_min, new_max;
    // Take care of translation
    new_min[0] = T[3][0];
    new_min[1] = T[3][1];
    new_min[2] = T[3][2];

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            a = T[i][j] * min_[j];
            b = T[i][j] * max_[j];
            if (a < b) {
                new_min[i] += a;
                new_max[i] += b;
            } else {
                new_min += b;
                new_max += a;
            }
        }
    }

    min_ = new_min;
    max_ = new_max;
}

glm::vec3 AABB::get_scale() const {
    return (max_ - min_);
}

glm::vec3 AABB::get_center() const {
    return (min_ + max_) * 0.5f;
}

glm::vec3 AABB::get_min() const {
    return min_;
}

glm::vec3 AABB::get_max() const {
    return max_;
}

void AABB::reset() {
    min_ = std::numeric_limits<glm::vec3>::max();
    max_ = std::numeric_limits<glm::vec3>::min();
};
}  // namespace W3D::SceneGraph