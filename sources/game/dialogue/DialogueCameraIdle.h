#pragma once

#include <raylib.h>

#include <array>
#include <cstdint>

namespace game {

struct DialogueCameraIdleSettings {
    Vector2 initialWaitSeconds{1.0f, 3.0f};
    Vector2 stillSeconds{3.0f, 5.0f};
    Vector2 movementSeconds{2.0f, 3.0f};
    Vector3 positionAmplitudeWorld{0.006f, 0.004f, 0.002f};
    Vector3 rotationAmplitudeDegrees{0.75f, 0.5f, 0.25f};
    float settleSeconds = 0.4f;
};

enum class DialogueCameraIdlePhase { Inactive, Waiting, Moving, Settling };

struct DialogueCameraIdleState {
    uint32_t randomState = 1;
    bool menuVisible = false;
    DialogueCameraIdlePhase phase = DialogueCameraIdlePhase::Inactive;
    float elapsed = 0.0f;
    float duration = 0.0f;
    // x/y/z translation followed by pitch/yaw/roll; sampled only at movement start.
    std::array<float, 6> amplitudes{};
    std::array<float, 6> frequencies{};
    std::array<float, 6> phases{};
    std::array<float, 6> offsets{};
    std::array<float, 6> velocities{};
    std::array<float, 6> settleOffsets{};
    std::array<float, 6> settleVelocities{};
    Vector3 positionOffsetLocal{};
    Vector3 rotationDegrees{};
};

// Reset presentation without restarting the session's random sequence.
void ClearDialogueCameraIdle(DialogueCameraIdleState& state);
void UpdateDialogueCameraIdle(DialogueCameraIdleState& state,
        const DialogueCameraIdleSettings& settings, bool menuVisible, bool frozen, float dt);

} // namespace game
