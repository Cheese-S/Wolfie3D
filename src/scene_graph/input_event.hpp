#pragma once

enum class EventSource { Keyboard, Mouse };

enum class MouseButton { Left, Right, Middle };

enum class KeyCode { W, S, A, D };

enum class KeyAction { Down, Up, Repeat };

enum class MouseAction { Down, Up, Move };

struct InputEvent {
    EventSource source;
};

struct KeyInputEvent : InputEvent {
    KeyCode code;
    KeyAction action;
};

struct MouseButtonInputEvent : InputEvent {
    MouseButton button;
    MouseAction action;
    float pos_x;
    float pos_y;
};