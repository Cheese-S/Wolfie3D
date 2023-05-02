#include "perspective_camera.hpp"

namespace W3D::SceneGraph {
PerspectiveCamera::PerspectiveCamera(const std::string &name) : Camera(name) {
}

glm::mat4 PerspectiveCamera::get_projection() {
    return glm::perspective(fov_, aspect_ratio_, znear_, zfar_);
}

void PerspectiveCamera::set_aspect_ratio(float aspect_ratio) {
    aspect_ratio_ = aspect_ratio;
}

void PerspectiveCamera::set_field_of_view(float fov) {
    fov_ = fov;
}
void PerspectiveCamera::set_far_plane(float zfar) {
    zfar_ = zfar;
}
void PerspectiveCamera::set_near_plane(float znear) {
    znear = znear_;
}
float PerspectiveCamera::get_far_plane() const {
    return zfar_;
}
float PerspectiveCamera::get_near_plane() const {
    return znear_;
}
float PerspectiveCamera::get_field_of_view() const {
    return fov_;
}
float PerspectiveCamera::get_aspect_ratio() const {
    return aspect_ratio_;
}

}  // namespace W3D::SceneGraph