#include "player.hpp"

namespace W3D::sg
{

const float Player::TRANSLATION_MOVE_STEP = 5.0f;

Player::Player(Node &node) :
    NodeScript(node)
{
}

// This method is called every frame. Responsible for updating a node's state
void Player::update(float delta_time)
{
	glm::vec3 delta_translation(0.0f, 0.0f, 0.0f);

	// Caveat: The models are rotated! Therefore, we translate the objects in model space in z-direction
	if (key_pressed_[KeyCode::eW])
	{
		delta_translation.z += TRANSLATION_MOVE_STEP;
	}

	if (key_pressed_[KeyCode::eS])
	{
		delta_translation.z -= TRANSLATION_MOVE_STEP;
	}

	if (key_pressed_[KeyCode::eA])
	{
		delta_translation.x -= TRANSLATION_MOVE_STEP;
	}

	if (key_pressed_[KeyCode::eD])
	{
		delta_translation.x += TRANSLATION_MOVE_STEP;
	}

	delta_translation *= speed_multiplier_ * delta_time;

	auto &T = get_node().get_transform();
	T.set_tranlsation(T.get_translation() + delta_translation);
}

void Player::process_event(const Event &event)
{
	if (event.type == EventType::eKeyInput)
	{
		const auto &key_event = static_cast<const KeyInputEvent &>(event);

		if (key_event.action == KeyAction::eDown || key_event.action == KeyAction::eRepeat)
		{
			key_pressed_[key_event.code] = true;
		}
		else
		{
			key_pressed_[key_event.code] = false;
		}
	}
}
}        // namespace W3D::sg