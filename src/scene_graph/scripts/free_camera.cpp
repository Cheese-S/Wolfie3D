#include "free_camera.hpp"

#include "scene_graph/components/camera.hpp"
#include "scene_graph/components/perspective_camera.hpp"

namespace W3D::SceneGraph {
const float FreeCamera::ROTATION_MOVE_WEIGHT = 2.4f;

const float FreeCamera::TRANSLATION_MOVE_STEP = 10.0f;

const float FreeCamera::TRANSLATION_MOVE_WEIGHT = 10.0f;

const uint32_t FreeCamera::TRANSLATION_MOVE_SPEED = 4;

FreeCamera::FreeCamera(Node &node) : NodeScript(node, "FreeCamera") {
}

void FreeCamera::update(float delta_time) {
    glm::vec3 delta_translation(0.0f, 0.0f, 0.0f);
    glm::vec3 delta_rotation(0.0f, 0.0f, 0.0f);

    if (key_pressed_[KeyCode::W]) {
        delta_translation.z -= TRANSLATION_MOVE_STEP;
    }

    if (key_pressed_[KeyCode::S]) {
        delta_translation.z += TRANSLATION_MOVE_STEP;
    }

    if (key_pressed_[KeyCode::A]) {
        delta_translation.x -= TRANSLATION_MOVE_STEP;
    }

    if (key_pressed_[KeyCode::D]) {
        delta_translation.x += TRANSLATION_MOVE_STEP;
    }

    if (mouse_button_pressed_[MouseButton::Middle]) {
        delta_translation.x += TRANSLATION_MOVE_WEIGHT * mouse_move_delta_.x;
        delta_translation.y += TRANSLATION_MOVE_WEIGHT * -mouse_move_delta_.y;
    } else if (mouse_button_pressed_[MouseButton::Left]) {
        delta_rotation.x -= ROTATION_MOVE_WEIGHT * mouse_move_delta_.y;
        delta_rotation.y -= ROTATION_MOVE_WEIGHT * mouse_move_delta_.x;
    }

    delta_translation *= speed_multiplier_ * delta_time;
    delta_rotation *= delta_time;

    if (delta_rotation != glm::vec3(0.0f) || delta_translation != glm::vec3(0.0f)) {
        auto &transform = get_node().get_component<Transform>();
        glm::quat qx = glm::angleAxis(delta_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat qy = glm::angleAxis(delta_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::quat orientation = glm::normalize(qy * transform.get_rotation() * qx);
        transform.set_tranlsation(transform.get_translation() +
                                  delta_translation * glm::conjugate(orientation));
        transform.set_rotation(orientation);
    }

    mouse_move_delta_ = {};
}

void FreeCamera::process_input_event(const InputEvent &input_event) {
    if (input_event.source == EventSource::Keyboard) {
        const auto &key_event = static_cast<const KeyInputEvent &>(input_event);

        if (key_event.action == KeyAction::Down || key_event.action == KeyAction::Repeat) {
            key_pressed_[key_event.code] = true;

        } else {
            key_pressed_[key_event.code] = false;
        }

    } else if (input_event.source == EventSource::Mouse) {
        const auto &mouse_event = static_cast<const MouseButtonInputEvent &>(input_event);
        glm::vec2 mouse_pos{std::floor(mouse_event.xpos), std::floor(mouse_event.ypos)};
        switch (mouse_event.action) {
            case MouseAction::Down:
                mouse_button_pressed_[mouse_event.button] = true;
                break;
            case MouseAction::Up:
                mouse_button_pressed_[mouse_event.button] = false;
                break;
            case MouseAction::Move:
                mouse_move_delta_ = mouse_pos - mouse_last_pos_;
                mouse_last_pos_ = mouse_pos;
                break;
            default:
                break;
        }
    }
}

void FreeCamera::resize(uint32_t width, uint32_t height) {
    auto &camera_node = get_node();

    if (camera_node.has_component<Camera>()) {
        if (auto camera = dynamic_cast<PerspectiveCamera *>(&camera_node.get_component<Camera>())) {
            camera->set_aspect_ratio(static_cast<float>(width) / height);
        }
    }
};
}  // namespace W3D::SceneGraph
