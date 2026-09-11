#include "game/dialogue/DialogueCameraIdle.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {
constexpr float Pi = 3.14159265358979323846f;

float RandomRange(DialogueCameraIdleState& state, float minimum, float maximum)
{
    // Local PRNG: camera variations must not perturb gameplay/speech randomness.
    uint32_t value = state.randomState ? state.randomState : 1;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state.randomState = value;
    const float unit = static_cast<float>(value >> 8) / 16777216.0f;
    return minimum + (maximum - minimum) * unit;
}

void Wait(DialogueCameraIdleState& state, Vector2 range)
{
    state.phase = DialogueCameraIdlePhase::Waiting;
    state.elapsed = 0.0f;
    state.duration = std::max(0.001f, RandomRange(state, range.x, range.y));
}

void StartMovement(DialogueCameraIdleState& state, const DialogueCameraIdleSettings& settings)
{
    state.phase = DialogueCameraIdlePhase::Moving;
    state.elapsed = 0.0f;
    state.duration = std::max(0.001f,
            RandomRange(state, settings.movementSeconds.x, settings.movementSeconds.y));
    const std::array<float, 6> maximums{
            settings.positionAmplitudeWorld.x, settings.positionAmplitudeWorld.y,
            settings.positionAmplitudeWorld.z, settings.rotationAmplitudeDegrees.x,
            settings.rotationAmplitudeDegrees.y, settings.rotationAmplitudeDegrees.z};
    for (size_t i = 0; i < maximums.size(); ++i) {
        state.amplitudes[i] = maximums[i] * RandomRange(state, 0.5f, 1.0f);
        // Less than one slow oscillation per gesture, with independent axis timing.
        state.frequencies[i] = RandomRange(state, 0.35f, 0.8f) * 2.0f * Pi;
        state.phases[i] = RandomRange(state, 0.0f, 2.0f * Pi);
    }
}

void SampleMovement(DialogueCameraIdleState& state)
{
    const float u = std::clamp(state.elapsed / state.duration, 0.0f, 1.0f);
    const float sine = std::sin(Pi * u);
    const float envelope = sine * sine;
    const float envelopeDerivative = Pi * std::sin(2.0f * Pi * u);
    for (size_t i = 0; i < state.offsets.size(); ++i) {
        const float angle = state.frequencies[i] * u + state.phases[i];
        state.offsets[i] = state.amplitudes[i] * envelope * std::sin(angle);
        state.velocities[i] = state.amplitudes[i] / state.duration
                * (envelopeDerivative * std::sin(angle)
                        + envelope * state.frequencies[i] * std::cos(angle));
    }
}

void SampleSettling(DialogueCameraIdleState& state)
{
    const float u = std::clamp(state.elapsed / state.duration, 0.0f, 1.0f);
    // Cubic Hermite return preserves the interrupted gesture's position and
    // velocity and reaches exactly zero position and velocity at the end.
    const float positionWeight = (2.0f * u - 3.0f) * u * u + 1.0f;
    const float velocityWeight = ((u - 2.0f) * u + 1.0f) * u;
    for (size_t i = 0; i < state.offsets.size(); ++i) {
        state.offsets[i] = positionWeight * state.settleOffsets[i]
                + velocityWeight * state.duration * state.settleVelocities[i];
        state.velocities[i] = (6.0f * u * u - 6.0f * u) / state.duration * state.settleOffsets[i]
                + (3.0f * u * u - 4.0f * u + 1.0f) * state.settleVelocities[i];
    }
}
}

void ClearDialogueCameraIdle(DialogueCameraIdleState& state)
{
    const uint32_t randomState = state.randomState;
    state = {};
    state.randomState = randomState;
}

void UpdateDialogueCameraIdle(DialogueCameraIdleState& state,
        const DialogueCameraIdleSettings& settings, bool menuVisible, bool frozen, float rawDt)
{
    if (frozen) return;
    const float dt = std::isfinite(rawDt) ? std::clamp(rawDt, 0.0f, 0.25f) : 0.0f;
    if (menuVisible != state.menuVisible) {
        state.menuVisible = menuVisible;
        if (menuVisible) {
            if (state.phase != DialogueCameraIdlePhase::Settling)
                Wait(state, settings.initialWaitSeconds);
        } else if (state.phase == DialogueCameraIdlePhase::Moving) {
            state.phase = DialogueCameraIdlePhase::Settling;
            state.elapsed = 0.0f;
            state.duration = std::max(0.001f, settings.settleSeconds);
            state.settleOffsets = state.offsets;
            state.settleVelocities = state.velocities;
        } else if (state.phase != DialogueCameraIdlePhase::Settling) {
            ClearDialogueCameraIdle(state);
        }
    }

    float remaining = dt;
    while (remaining > 0.0f && state.phase != DialogueCameraIdlePhase::Inactive) {
        const float step = std::min(remaining, state.duration - state.elapsed);
        state.elapsed += step;
        remaining -= step;
        if (state.elapsed < state.duration) {
            if (state.phase == DialogueCameraIdlePhase::Moving) SampleMovement(state);
            else if (state.phase == DialogueCameraIdlePhase::Settling) SampleSettling(state);
            break;
        }
        if (state.phase == DialogueCameraIdlePhase::Waiting) {
            StartMovement(state, settings);
        } else {
            const bool settled = state.phase == DialogueCameraIdlePhase::Settling;
            state.offsets = {};
            state.velocities = {};
            if (state.menuVisible) Wait(state, settled ? settings.initialWaitSeconds : settings.stillSeconds);
            else ClearDialogueCameraIdle(state);
        }
    }
    state.positionOffsetLocal = {state.offsets[0], state.offsets[1], state.offsets[2]};
    state.rotationDegrees = {state.offsets[3], state.offsets[4], state.offsets[5]};
}

} // namespace game
