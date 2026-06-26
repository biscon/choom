#include "sector_demo/SectorFreeflyController.h"

#include <raylib.h>
#include <raymath.h>

#include <cmath>

namespace game {

namespace {

constexpr float MouseSensitivity = 0.0030f;
constexpr float MoveSpeed = 5.0f;
constexpr float PitchLimit = 1.45f;
constexpr int MouseLookWarmupFrames = 2;

} // namespace

void ResetSectorFreeflyController(
        SectorFreeflyControllerState& state,
        const SectorViewPose& pose)
{
    state.pose = pose;
    state.mouseLookEnabled = true;
    state.mouseLookWarmupFrames = 0;
}

void EnterSectorFreeflyController(SectorFreeflyControllerState& state)
{
    state.mouseLookEnabled = true;
    state.mouseLookWarmupFrames = MouseLookWarmupFrames;
    DisableCursor();
}

void LeaveSectorFreeflyController()
{
    EnableCursor();
}

void SetSectorFreeflyMouseLookEnabled(
        SectorFreeflyControllerState& state,
        bool enabled)
{
    state.mouseLookEnabled = enabled;
    if (state.mouseLookEnabled) {
        state.mouseLookWarmupFrames = MouseLookWarmupFrames;
        DisableCursor();
    } else {
        EnableCursor();
    }
}

void UpdateSectorFreeflyController(
        SectorFreeflyControllerState& state,
        engine::Input& input,
        float dt)
{
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&state](engine::InputEvent& event) {
                if (event.key.key != KEY_F11) {
                    return;
                }

                SetSectorFreeflyMouseLookEnabled(state, !state.mouseLookEnabled);
                engine::ConsumeEvent(event);
            }
    );

    if (state.mouseLookEnabled) {
        if (state.mouseLookWarmupFrames > 0) {
            --state.mouseLookWarmupFrames;
        } else {
            const Vector2 mouseDelta = input.MouseDelta();
            state.pose.yawRadians += mouseDelta.x * MouseSensitivity;
            state.pose.pitchRadians -= mouseDelta.y * MouseSensitivity;
            state.pose.pitchRadians = Clamp(state.pose.pitchRadians, -PitchLimit, PitchLimit);
        }
    }

    if (state.mouseLookEnabled) {
        const Vector3 forward{
                std::cos(state.pose.yawRadians),
                0.0f,
                std::sin(state.pose.yawRadians)};
        const Vector3 right{-forward.z, 0.0f, forward.x};
        Vector3 movement{};

        if (input.IsKeyDown(KEY_W)) {
            movement = Vector3Add(movement, forward);
        }
        if (input.IsKeyDown(KEY_S)) {
            movement = Vector3Subtract(movement, forward);
        }
        if (input.IsKeyDown(KEY_D)) {
            movement = Vector3Add(movement, right);
        }
        if (input.IsKeyDown(KEY_A)) {
            movement = Vector3Subtract(movement, right);
        }
        if (input.IsKeyDown(KEY_SPACE)) {
            movement.y += 1.0f;
        }
        if (input.IsKeyDown(KEY_LEFT_CONTROL) || input.IsKeyDown(KEY_RIGHT_CONTROL)) {
            movement.y -= 1.0f;
        }

        if (Vector3LengthSqr(movement) > 0.0001f) {
            movement = Vector3Normalize(movement);
            state.pose.position = Vector3Add(
                    state.pose.position,
                    Vector3Scale(movement, MoveSpeed * dt));
        }
    }
}

} // namespace game
