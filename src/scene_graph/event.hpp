#pragma once

namespace W3D
{
enum class EventType
{
	eKeyInput,
	eMouseButton,
	eResize
};

enum class MouseButton
{
	eLeft,
	eRight,
	eMiddle,
	eUnknown
};

enum class KeyCode
{
	eUnknown,

	eW,
	eS,
	eA,
	eD,

	e1,
	e2,
	e3,
};

enum class KeyAction
{
	eDown,
	eUp,
	eRepeat,
	eUnknown
};

enum class MouseAction
{
	eDown,
	eUp,
	eMove,
	eUnknown
};

struct Event
{
	Event(EventType type) :
	    type(type){};
	EventType type;
};

struct ResizeEvent : Event
{
	ResizeEvent() :
	    Event(EventType::eResize){};
};

struct KeyInputEvent : Event
{
	KeyInputEvent(KeyCode code, KeyAction action) :
	    Event(EventType::eKeyInput),
	    code(code),
	    action(action)
	{
	}
	KeyCode   code;
	KeyAction action;
};

struct MouseButtonInputEvent : Event
{
	MouseButtonInputEvent(MouseButton button, MouseAction action, float pos_x, float pos_y) :
	    Event(EventType::eMouseButton),
	    button(button),
	    action(action),
	    xpos(pos_x),
	    ypos(pos_y)
	{
	}
	MouseButton button;
	MouseAction action;
	float       xpos;
	float       ypos;
};

}        // namespace W3D