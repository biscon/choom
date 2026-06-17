#pragma once

#include <raylib.h>

#include <cstdint>

namespace engine {

enum class InputEventType {
    MouseButtonPressed,
    MouseButtonReleased,
    MouseClick,
    KeyPressed,
    KeyReleased,
    KeyRepeated,
    TextInput,
    MouseWheel,
    Any
};

struct MouseButtonEvent {
    Vector2 position = {};
    int button = 0;
};

struct MouseClickEvent {
    Vector2 pressPosition = {};
    Vector2 releasePosition = {};
    int button = 0;
    bool doubleClick = false;
};

struct KeyEvent {
    int key = 0;
};

struct TextInputEvent {
    uint32_t codepoint = 0;
};

struct MouseWheelEvent {
    float value = 0.0f;
};

struct InputEvent {
    InputEventType type = InputEventType::Any;
    bool handled = false;

    union {
        MouseButtonEvent mouseButton;
        MouseClickEvent mouseClick;
        KeyEvent key;
        TextInputEvent text;
        MouseWheelEvent wheel;
    };
};

inline void ConsumeEvent(InputEvent& event)
{
    event.handled = true;
}

} // namespace engine
