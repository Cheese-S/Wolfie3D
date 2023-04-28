#pragma once

#include "common/glm_common.hpp"
#include "scene_graph/script.hpp"

namespace W3D::SceneGraph {
class FreeCamera : public NodeScript {
   public:
    static const float ROTATION_MOVE_WEIGHT;
    static const float TRANSLATION_MOVE_STEP;
    static const float TRANSLATION_MOVE_WEIGHT;
    static const uint32_t TRANSLATION_MOVE_SPEED;

    FreeCamera(Node& node);
    virtual ~FreeCamera() = default;
    virtual void update(float delta_time) override;
    virtual void process_input_event(const InputEvent& input_event) override;
    virtual void resize(uint32_t width, uint32_t height) override;

   private:
    float speed_multiplier_ = 3.0f;
    glm::vec2 mouse_move_delta_{0.0f};
    glm::vec2 mouse_last_pos_{0.0f};

    std::unordered_map<KeyCode, bool> key_pressed_;
    std::unordered_map<MouseButton, bool> mouse_button_pressed_;
};
}  // namespace W3D::SceneGraph