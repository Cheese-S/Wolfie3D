#pragma once

namespace W3D
{
struct Event;
enum class KeyCode;
namespace sg
{
class Node;

}

enum class ControllerMode
{
	eCamera,
	ePlayer1,
	ePlayer2,
};

class Controller
{
  public:
	Controller(sg::Node &camera_node, sg::Node &player_1_node, sg::Node &player_2_node);
	void process_event(const Event &event);
	bool are_players_colliding();

  private:
	void switch_mode(KeyCode code);
	void deliver_event(const Event &event);

	sg::Node      &camera_;
	sg::Node      &player_1;
	sg::Node      &player_2;
	ControllerMode mode_;
};
}        // namespace W3D