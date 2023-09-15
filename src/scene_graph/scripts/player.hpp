#pragma once

#include "scene_graph/script.hpp"

namespace W3D::sg
{
class Player : public NodeScript
{
  public:
	static const float TRANSLATION_MOVE_STEP;

	Player(Node &node);

	void update(float delta_time) override;
	void process_event(const Event &event) override;

  private:
	float speed_multiplier_ = 2.0f;

	std::unordered_map<KeyCode, bool> key_pressed_;
};
}        // namespace W3D::sg