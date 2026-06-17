# Input System

The engine input system is owned by `engine::Input`. It polls raw raylib input
once per frame, stores continuous input state, and converts input transitions
into per-frame events.

Input is an engine service, not ECS component data. Systems receive input
explicitly when they need it, usually through `EngineContext`.

## Core Rules

- Call `Input::Initialize()` during startup.
- Call `Input::ReserveEvents()` during initialization to avoid frame-time event
  vector growth.
- Call `Input::BeginFrame()` once per frame before polling.
- Call `Input::PollRaylib(dt)` once per frame after `BeginFrame()`.
- Use events for presses, releases, clicks, text input, key repeat, and wheel
  movement.
- Use current input state for continuous movement, aiming, dragging, and held
  buttons.
- UI should consume input events before game systems read them.
- Game systems should normally read only unhandled events.
- Do not store input in ECS components.

## Initialization And Frame Polling

`EngineContext` owns the engine services, including `Input`.

```cpp
#include "engine/EngineContext.h"

int main()
{
    InitWindow(1280, 720, "Game");

    engine::EngineContext context;
    context.input.Initialize();
    context.input.ReserveEvents(256);

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();

        context.input.BeginFrame();
        context.input.PollRaylib(dt);

        // Intended input consumption order:
        // 1. UI consumes unhandled events.
        // 2. Debug console/overlay consumes unhandled events.
        // 3. Game systems consume remaining unhandled events.

        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

`BeginFrame()` clears only the current frame event list and resets per-frame
mouse delta and wheel state. It does not reset held keys, held mouse buttons,
double-click tracking, or key-repeat state.

`PollRaylib(dt)` reads raylib input and appends events for this frame.

## Events

Input events are stored in `std::vector<InputEvent>` and are valid for the
current frame only.

```cpp
const std::vector<engine::InputEvent>& events = input.Events();
for (const engine::InputEvent& event : events) {
    if (event.type == engine::InputEventType::KeyPressed) {
        // event.key.key contains the raylib key code.
    }
}
```

Prefer `ForEachEvent()` when filtering by type or when respecting event
consumption:

```cpp
input.ForEachEvent(
        engine::InputEventType::KeyPressed,
        true,
        [](const engine::InputEvent& event) {
            if (event.key.key == KEY_SPACE) {
                // Jump, confirm, or interact.
            }
        }
);
```

The second argument is `unhandledOnly`. Pass `true` when a system should skip
events already consumed by UI or another earlier system.

## Consuming Events

Events contain a `handled` flag. Earlier systems can mark events as handled so
later systems ignore them.

```cpp
void UiInputSystem(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [](engine::InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (PointInsideButton(event.mouseClick.releasePosition)) {
                    ActivateButton();
                    engine::ConsumeEvent(event);
                }
            }
    );
}
```

Game systems should usually read only unhandled events:

```cpp
void GameplayInputSystem(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [](const engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    OpenPauseMenu();
                }
            }
    );
}
```

This keeps UI clicks, text-entry keys, and debug-console commands from also
triggering gameplay actions.

## Event Types

`InputEventType` identifies which union payload is active.

| Type | Payload | Use |
| --- | --- | --- |
| `MouseButtonPressed` | `event.mouseButton` | Mouse button down transitions. |
| `MouseButtonReleased` | `event.mouseButton` | Mouse button up transitions. |
| `MouseClick` | `event.mouseClick` | Clicks that did not move beyond the configured threshold. |
| `KeyPressed` | `event.key` | Key press transitions from `GetKeyPressed()`. |
| `KeyReleased` | `event.key` | Key release transitions for tracked keys. |
| `KeyRepeated` | `event.key` | Key-repeat events generated from held tracked keys. |
| `TextInput` | `event.text` | Text codepoints from `GetCharPressed()`. |
| `MouseWheel` | `event.wheel` | Mouse wheel movement for this frame. |
| `Any` | depends | Used only as a filter when iterating events. |

Mouse click events include both press and release positions and a double-click
flag:

```cpp
input.ForEachEvent(
        engine::InputEventType::MouseClick,
        true,
        [](const engine::InputEvent& event) {
            const Vector2 press = event.mouseClick.pressPosition;
            const Vector2 release = event.mouseClick.releasePosition;
            const bool doubleClick = event.mouseClick.doubleClick;

            if (doubleClick) {
                SelectWordAt(release);
            } else {
                SelectObjectAt(release);
            }
        }
);
```

Text input events contain Unicode codepoints from raylib:

```cpp
input.ForEachEvent(
        engine::InputEventType::TextInput,
        true,
        [](const engine::InputEvent& event) {
            AppendCodepointToTextBuffer(event.text.codepoint);
        }
);
```

## Continuous State

Use continuous state APIs for behavior that depends on what is held right now.
Do not use key press events for continuous movement.

```cpp
Vector2 movement{};

if (input.IsKeyDown(KEY_A)) {
    movement.x -= 1.0f;
}
if (input.IsKeyDown(KEY_D)) {
    movement.x += 1.0f;
}
if (input.IsKeyDown(KEY_W)) {
    movement.y -= 1.0f;
}
if (input.IsKeyDown(KEY_S)) {
    movement.y += 1.0f;
}

if (movement.x != 0.0f || movement.y != 0.0f) {
    movement = Vector2Normalize(movement);
}
```

Mouse helpers return frame state captured during `PollRaylib()`:

```cpp
const Vector2 mousePosition = input.MousePosition();
const Vector2 mouseDelta = input.MouseDelta();
const float wheel = input.MouseWheelMove();

if (input.IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
    cameraYaw += mouseDelta.x * 0.01f;
}

zoom -= wheel * 0.1f;
```

`MouseWheelMove()` is also emitted as a `MouseWheel` event. Use the event when
consumption order matters, and use the state helper when only the final wheel
value for the frame matters.

## Configuration

`InputConfig` controls click and key-repeat behavior.

```cpp
engine::InputConfig& config = input.Config();
config.doubleClickThreshold = 0.25f;
config.clickMaxMoveDistance = 6.0f;
config.keyRepeatInitialDelay = 0.40f;
config.keyRepeatInterval = 0.05f;
```

`doubleClickThreshold` is the maximum time between clicks for a double-click.
`clickMaxMoveDistance` is the maximum pointer movement between press and release
for a click. `keyRepeatInitialDelay` is how long a tracked key must be held
before repeat events begin. `keyRepeatInterval` is the time between repeat
events after the initial delay.

## Allocation Notes

Reserve enough event capacity during initialization:

```cpp
context.input.Initialize();
context.input.ReserveEvents(256);
```

If the event vector reaches capacity, input still works, but the engine prints:

```text
[Input WARNING] Event capacity exceeded. Dynamic allocation may occur. Did you forget to call Input::ReserveEvents()?
```

Increase the reserved capacity if this warning appears during normal gameplay.
The input system uses fixed arrays for tracked key-repeat and mouse-button
state, avoiding per-frame maps.

## Raylib Key And Mouse Codes

The input system uses raylib key and mouse button constants directly, such as
`KEY_SPACE`, `KEY_ESCAPE`, `MOUSE_LEFT_BUTTON`, and `MOUSE_RIGHT_BUTTON`.

Tracked keys are raylib key codes from `0` through `511`. Tracked mouse buttons
are button codes from `0` through `7`. Keys or buttons outside those ranges
fall back to raylib state queries where possible, but key-release and key-repeat
events are generated only for tracked keys.
