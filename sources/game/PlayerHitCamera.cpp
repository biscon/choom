#include "game/PlayerHitCamera.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {

bool Finite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

void AdvanceSpringAxis(
        float& offset,
        float& velocity,
        float frequencyHz,
        float dampingRatio,
        float deltaSeconds)
{
    const float angularFrequency = 2.0f * PI * frequencyHz;
    float remaining = deltaSeconds;
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 1.0f / 240.0f);
        const float acceleration = -angularFrequency * angularFrequency * offset
                - 2.0f * dampingRatio * angularFrequency * velocity;
        velocity += acceleration * step;
        offset += velocity * step;
        remaining -= step;
    }
    if (std::fabs(offset) < 0.000001f
            && std::fabs(velocity) < 0.00001f) {
        offset = 0.0f;
        velocity = 0.0f;
    }
}

void ClampSpringAxis(float& offset, float& velocity, float limit)
{
    if (!(limit > 0.0f)) {
        offset = 0.0f;
        velocity = 0.0f;
        return;
    }
    if (offset > limit) {
        offset = limit;
        if (velocity > 0.0f) velocity = 0.0f;
    } else if (offset < -limit) {
        offset = -limit;
        if (velocity < 0.0f) velocity = 0.0f;
    }
}

} // namespace

void ClearPlayerHitCamera(PlayerHitCameraState& state)
{
    state = PlayerHitCameraState{};
}

void ApplyPlayerHitCameraImpulse(
        PlayerHitCameraState& state,
        const NpcAttackCameraImpactDefinition& sourceDefinition,
        Vector2 directionFromAttackerToPlayerWorld,
        float playerYawRadians)
{
    if (!sourceDefinition.enabled) return;
    if (!Finite(state.rotationDegrees)
            || !Finite(state.rotationVelocityDegrees)) {
        ClearPlayerHitCamera(state);
    }

    const float pitchKick = std::clamp(
            std::isfinite(sourceDefinition.pitchKickDegrees)
                    ? sourceDefinition.pitchKickDegrees : 0.0f,
            0.0f,
            kMaximumNpcAttackCameraImpactKickDegrees);
    const float rollKick = std::clamp(
            std::isfinite(sourceDefinition.rollKickDegrees)
                    ? sourceDefinition.rollKickDegrees : 0.0f,
            0.0f,
            kMaximumNpcAttackCameraImpactKickDegrees);
    state.springFrequencyHz = std::clamp(
            std::isfinite(sourceDefinition.springFrequencyHz)
                    ? sourceDefinition.springFrequencyHz
                    : kDefaultNpcAttackCameraImpactSpringFrequencyHz,
            kMinimumNpcAttackCameraImpactSpringFrequencyHz,
            kMaximumNpcAttackCameraImpactSpringFrequencyHz);
    state.springDampingRatio = std::clamp(
            std::isfinite(sourceDefinition.springDampingRatio)
                    ? sourceDefinition.springDampingRatio
                    : kDefaultNpcAttackCameraImpactSpringDampingRatio,
            kMinimumNpcAttackCameraImpactSpringDampingRatio,
            kMaximumNpcAttackCameraImpactSpringDampingRatio);
    state.maxPitchDegrees = std::clamp(
            std::isfinite(sourceDefinition.maxPitchDegrees)
                    ? sourceDefinition.maxPitchDegrees : 0.0f,
            0.0f,
            kMaximumNpcAttackCameraImpactLimitDegrees);
    state.maxRollDegrees = std::clamp(
            std::isfinite(sourceDefinition.maxRollDegrees)
                    ? sourceDefinition.maxRollDegrees : 0.0f,
            0.0f,
            kMaximumNpcAttackCameraImpactLimitDegrees);

    Vector3 kick{pitchKick, 0.0f, 0.0f};
    if (Finite(directionFromAttackerToPlayerWorld)) {
        const float length = std::hypot(
                directionFromAttackerToPlayerWorld.x,
                directionFromAttackerToPlayerWorld.y);
        if (length > 0.0001f) {
            const Vector2 playerToAttacker{
                    -directionFromAttackerToPlayerWorld.x / length,
                    -directionFromAttackerToPlayerWorld.y / length};
            const float yaw = std::isfinite(playerYawRadians)
                    ? playerYawRadians : 0.0f;
            const Vector2 forward{std::cos(yaw), std::sin(yaw)};
            const Vector2 right{-forward.y, forward.x};
            kick.x = (playerToAttacker.x * forward.x
                    + playerToAttacker.y * forward.y) * pitchKick;
            kick.z = (playerToAttacker.x * right.x
                    + playerToAttacker.y * right.y) * rollKick;
        }
    }

    state.rotationDegrees.x = std::clamp(
            state.rotationDegrees.x + kick.x,
            -state.maxPitchDegrees,
            state.maxPitchDegrees);
    state.rotationDegrees.y = 0.0f;
    state.rotationDegrees.z = std::clamp(
            state.rotationDegrees.z + kick.z,
            -state.maxRollDegrees,
            state.maxRollDegrees);
}

void UpdatePlayerHitCamera(
        PlayerHitCameraState& state,
        float deltaSeconds)
{
    if (!Finite(state.rotationDegrees)
            || !Finite(state.rotationVelocityDegrees)
            || !std::isfinite(state.springFrequencyHz)
            || !std::isfinite(state.springDampingRatio)
            || !std::isfinite(state.maxPitchDegrees)
            || !std::isfinite(state.maxRollDegrees)) {
        ClearPlayerHitCamera(state);
        return;
    }
    const float dt = std::isfinite(deltaSeconds)
            ? std::clamp(deltaSeconds, 0.0f, 0.25f)
            : 0.0f;
    const float frequencyHz = std::clamp(
            state.springFrequencyHz,
            kMinimumNpcAttackCameraImpactSpringFrequencyHz,
            kMaximumNpcAttackCameraImpactSpringFrequencyHz);
    const float dampingRatio = std::clamp(
            state.springDampingRatio,
            kMinimumNpcAttackCameraImpactSpringDampingRatio,
            kMaximumNpcAttackCameraImpactSpringDampingRatio);
    AdvanceSpringAxis(
            state.rotationDegrees.x,
            state.rotationVelocityDegrees.x,
            frequencyHz,
            dampingRatio,
            dt);
    AdvanceSpringAxis(
            state.rotationDegrees.z,
            state.rotationVelocityDegrees.z,
            frequencyHz,
            dampingRatio,
            dt);
    state.rotationDegrees.y = 0.0f;
    state.rotationVelocityDegrees.y = 0.0f;
    ClampSpringAxis(
            state.rotationDegrees.x,
            state.rotationVelocityDegrees.x,
            std::clamp(
                    state.maxPitchDegrees,
                    0.0f,
                    kMaximumNpcAttackCameraImpactLimitDegrees));
    ClampSpringAxis(
            state.rotationDegrees.z,
            state.rotationVelocityDegrees.z,
            std::clamp(
                    state.maxRollDegrees,
                    0.0f,
                    kMaximumNpcAttackCameraImpactLimitDegrees));
}

} // namespace game
