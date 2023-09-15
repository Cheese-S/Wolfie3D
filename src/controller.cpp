#include "controller.hpp"

#include "scene_graph/components/aabb.hpp"
#include "scene_graph/components/mesh.hpp"
#include "scene_graph/event.hpp"
#include "scene_graph/node.hpp"
#include "scene_graph/script.hpp"

namespace W3D
{
Controller::Controller(sg::Node &camera_node, sg::Node &player_1_node, sg::Node &player_2_node) :
    camera_(camera_node),
    player_1(player_1_node),
    player_2(player_2_node)
{
}

void Controller::process_event(const Event &event)
{
	if (event.type == EventType::eKeyInput)
	{
		const auto &key_input_event = static_cast<const KeyInputEvent &>(event);
		if (key_input_event.code > KeyCode::eD)
		{
			switch_mode(key_input_event.code);
			return;
		}
	}

	deliver_event(event);
}

void Controller::deliver_event(const Event &event)
{
	sg::Script *p_script;
	if (mode_ == ControllerMode::ePlayer1)
	{
		p_script = &player_1.get_component<sg::Script>();
	}
	else if (mode_ == ControllerMode::ePlayer2)
	{
		p_script = &player_2.get_component<sg::Script>();
	}
	else
	{
		p_script = &camera_.get_component<sg::Script>();
	}

	p_script->process_event(event);
}

bool Controller::are_players_colliding()
{
	glm::mat4 p1_M              = player_1.get_transform().get_world_M();
	glm::mat4 p2_M              = player_2.get_transform().get_world_M();
	sg::AABB  p1_transformed_bd = player_1.get_component<sg::Mesh>().get_bounds().transform(p1_M);
	sg::AABB  p2_transformed_bd = player_2.get_component<sg::Mesh>().get_bounds().transform(p2_M);
	return p1_transformed_bd.collides_with(p2_transformed_bd);
}

void Controller::switch_mode(KeyCode code)
{
	if (code == KeyCode::e1)
	{
		mode_ = ControllerMode::ePlayer1;
	}
	else if (code == KeyCode::e2)
	{
		mode_ = ControllerMode::ePlayer2;
	}
	else
	{
		mode_ = ControllerMode::eCamera;
	}
}
}        // namespace W3D