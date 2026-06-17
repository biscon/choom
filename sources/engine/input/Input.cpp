#include "engine/input/Input.h"

#include <cmath>
#include <cstdio>

namespace engine {

namespace {

float Distance(Vector2 a, Vector2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void Input::Initialize()
{
    Reset();
}

void Input::Reset()
{
    config = InputConfig{};
    events.clear();
    frameState = InputFrameState{};
    keyRepeatStates = {};
    mouseButtonStates = {};
}

void Input::ReserveEvents(size_t capacity)
{
    events.reserve(capacity);
}

void Input::BeginFrame()
{
    events.clear();
    frameState.previousMousePosition = frameState.mousePosition;
    frameState.mouseDelta = {};
    frameState.mouseWheelMove = 0.0f;
}

void Input::PollRaylib(float dt)
{
    frameState.mousePosition = GetMousePosition();
    frameState.mouseDelta = Vector2{
            frameState.mousePosition.x - frameState.previousMousePosition.x,
            frameState.mousePosition.y - frameState.previousMousePosition.y
    };

    frameState.mouseWheelMove = GetMouseWheelMove();
    if (frameState.mouseWheelMove != 0.0f) {
        InputEvent event{};
        event.type = InputEventType::MouseWheel;
        event.wheel = MouseWheelEvent{frameState.mouseWheelMove};
        AddEvent(event);
    }

    const int mouseButtons[] = {
            MOUSE_LEFT_BUTTON,
            MOUSE_RIGHT_BUTTON,
            MOUSE_MIDDLE_BUTTON
    };

    const float now = static_cast<float>(GetTime());
    for (int button : mouseButtons) {
        if (!IsTrackedMouseButton(button)) {
            continue;
        }

        MouseButtonState& state = mouseButtonStates[static_cast<size_t>(button)];

        if (IsMouseButtonPressed(button)) {
            state.down = true;
            state.pressPosition = frameState.mousePosition;
            state.pressTime = now;

            InputEvent event{};
            event.type = InputEventType::MouseButtonPressed;
            event.mouseButton = MouseButtonEvent{frameState.mousePosition, button};
            AddEvent(event);
        }

        if (IsMouseButtonReleased(button)) {
            state.down = false;

            InputEvent releaseEvent{};
            releaseEvent.type = InputEventType::MouseButtonReleased;
            releaseEvent.mouseButton = MouseButtonEvent{frameState.mousePosition, button};
            AddEvent(releaseEvent);

            if (Distance(state.pressPosition, frameState.mousePosition) <= config.clickMaxMoveDistance) {
                const bool doubleClick = state.lastClickTime >= 0.0f
                        && now - state.lastClickTime <= config.doubleClickThreshold;
                state.lastClickTime = now;

                InputEvent clickEvent{};
                clickEvent.type = InputEventType::MouseClick;
                clickEvent.mouseClick = MouseClickEvent{
                        state.pressPosition,
                        frameState.mousePosition,
                        button,
                        doubleClick
                };
                AddEvent(clickEvent);
            }
        }
    }

    int pressedKey = GetKeyPressed();
    while (pressedKey != 0) {
        InputEvent event{};
        event.type = InputEventType::KeyPressed;
        event.key = KeyEvent{pressedKey};
        AddEvent(event);

        if (IsTrackedKey(pressedKey)) {
            KeyRepeatState& state = keyRepeatStates[static_cast<size_t>(pressedKey)];
            state.down = true;
            state.heldTime = 0.0f;
            state.nextRepeatTime = config.keyRepeatInitialDelay;
        }

        pressedKey = GetKeyPressed();
    }

    for (size_t i = 0; i < keyRepeatStates.size(); ++i) {
        KeyRepeatState& state = keyRepeatStates[i];
        if (!state.down) {
            continue;
        }

        const int key = static_cast<int>(i);
        if (!::IsKeyDown(key)) {
            state = KeyRepeatState{};

            InputEvent event{};
            event.type = InputEventType::KeyReleased;
            event.key = KeyEvent{key};
            AddEvent(event);
            continue;
        }

        state.heldTime += dt;
        while (state.heldTime >= state.nextRepeatTime) {
            InputEvent event{};
            event.type = InputEventType::KeyRepeated;
            event.key = KeyEvent{key};
            AddEvent(event);

            state.nextRepeatTime += config.keyRepeatInterval;
            if (config.keyRepeatInterval <= 0.0f) {
                state.nextRepeatTime = state.heldTime + config.keyRepeatInitialDelay;
                break;
            }
        }
    }

    int codepoint = GetCharPressed();
    while (codepoint != 0) {
        InputEvent event{};
        event.type = InputEventType::TextInput;
        event.text = TextInputEvent{static_cast<uint32_t>(codepoint)};
        AddEvent(event);

        codepoint = GetCharPressed();
    }
}

InputConfig& Input::Config()
{
    return config;
}

const InputConfig& Input::Config() const
{
    return config;
}

const std::vector<InputEvent>& Input::Events() const
{
    return events;
}

std::vector<InputEvent>& Input::Events()
{
    return events;
}

bool Input::IsKeyDown(int key) const
{
    if (IsTrackedKey(key)) {
        return keyRepeatStates[static_cast<size_t>(key)].down;
    }

    return ::IsKeyDown(key);
}

bool Input::IsMouseButtonDown(int button) const
{
    if (IsTrackedMouseButton(button)) {
        return mouseButtonStates[static_cast<size_t>(button)].down;
    }

    return ::IsMouseButtonDown(button);
}

Vector2 Input::MousePosition() const
{
    return frameState.mousePosition;
}

Vector2 Input::MouseDelta() const
{
    return frameState.mouseDelta;
}

float Input::MouseWheelMove() const
{
    return frameState.mouseWheelMove;
}

bool Input::IsTrackedKey(int key)
{
    return key >= 0 && key < InputMaxTrackedKeys;
}

bool Input::IsTrackedMouseButton(int button)
{
    return button >= 0 && button < InputMaxMouseButtons;
}

void Input::AddEvent(InputEvent event)
{
    if (events.size() == events.capacity()) {
        std::fprintf(
                stderr,
                "[Input WARNING] Event capacity exceeded. Dynamic allocation may occur. Did you forget to call Input::ReserveEvents()?\n"
        );
    }

    events.push_back(event);
}

} // namespace engine
