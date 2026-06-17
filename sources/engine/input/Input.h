#pragma once

#include "engine/input/InputConfig.h"
#include "engine/input/InputEvents.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace engine {

static constexpr int InputMaxTrackedKeys = 512;
static constexpr int InputMaxMouseButtons = 8;

struct InputFrameState {
    Vector2 mousePosition = {};
    Vector2 previousMousePosition = {};
    Vector2 mouseDelta = {};
    float mouseWheelMove = 0.0f;
};

struct KeyRepeatState {
    bool down = false;
    float heldTime = 0.0f;
    float nextRepeatTime = 0.0f;
};

struct MouseButtonState {
    bool down = false;
    Vector2 pressPosition = {};
    float pressTime = 0.0f;
    float lastClickTime = -1.0f;
};

class Input {
public:
    void Initialize();
    void Reset();

    void ReserveEvents(size_t capacity);

    void BeginFrame();
    void PollRaylib(float dt);

    InputConfig& Config();
    const InputConfig& Config() const;

    const std::vector<InputEvent>& Events() const;
    std::vector<InputEvent>& Events();

    bool IsKeyDown(int key) const;
    bool IsMouseButtonDown(int button) const;

    Vector2 MousePosition() const;
    Vector2 MouseDelta() const;
    float MouseWheelMove() const;

    template <typename Func>
    void ForEachEvent(InputEventType type, bool unhandledOnly, Func func);

    template <typename Func>
    void ForEachEvent(InputEventType type, bool unhandledOnly, Func func) const;

private:
    static bool IsTrackedKey(int key);
    static bool IsTrackedMouseButton(int button);

    void AddEvent(InputEvent event);

    InputConfig config;
    InputFrameState frameState;
    std::vector<InputEvent> events;
    std::array<KeyRepeatState, InputMaxTrackedKeys> keyRepeatStates = {};
    std::array<MouseButtonState, InputMaxMouseButtons> mouseButtonStates = {};
};

template <typename Func>
void Input::ForEachEvent(InputEventType type, bool unhandledOnly, Func func)
{
    for (InputEvent& event : events) {
        if (type != InputEventType::Any && event.type != type) {
            continue;
        }
        if (unhandledOnly && event.handled) {
            continue;
        }

        func(event);
    }
}

template <typename Func>
void Input::ForEachEvent(InputEventType type, bool unhandledOnly, Func func) const
{
    for (const InputEvent& event : events) {
        if (type != InputEventType::Any && event.type != type) {
            continue;
        }
        if (unhandledOnly && event.handled) {
            continue;
        }

        func(event);
    }
}

} // namespace engine
