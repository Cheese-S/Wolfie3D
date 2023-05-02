#pragma once

namespace W3D {
enum class EventSource { Keyboard, Mouse };

enum class MouseButton { Left, Right, Middle, Unknown };

enum class KeyCode { W, S, A, D, Unknown };

enum class KeyAction { Down, Up, Repeat, Unknown };

enum class MouseAction { Down, Up, Move, Unknown };

struct InputEvent {
    InputEvent(EventSource source) : source(source){};
    EventSource source;
};

struct KeyInputEvent : InputEvent {
    KeyInputEvent(KeyCode code, KeyAction action)
        : InputEvent(EventSource::Keyboard), code(code), action(action) {
    }
    KeyCode code;
    KeyAction action;
};

struct MouseButtonInputEvent : InputEvent {
    MouseButtonInputEvent(MouseButton button, MouseAction action, float pos_x, float pos_y)
        : InputEvent(EventSource::Mouse), button(button), action(action), xpos(pos_x), ypos(pos_y) {
    }
    MouseButton button;
    MouseAction action;
    float xpos;
    float ypos;
};

}  // namespace W3D