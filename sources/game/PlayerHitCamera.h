#pragma once

#include "game/npc/NpcDefinitions.h"

#include <raylib.h>

namespace game {

struct PlayerHitCameraState {
    Vector3 rotationDegrees{};
    Vector3 rotationVelocityDegrees{};
    float springFrequencyHz =
            kDefaultNpcAttackCameraImpactSpringFrequencyHz;
    float springDampingRatio =
            kDefaultNpcAttackCameraImpactSpringDampingRatio;
    float maxPitchDegrees = kDefaultNpcAttackCameraImpactMaxPitchDegrees;
    float maxRollDegrees = kDefaultNpcAttackCameraImpactMaxRollDegrees;
};

void ClearPlayerHitCamera(PlayerHitCameraState& state);
void ApplyPlayerHitCameraImpulse(
        PlayerHitCameraState& state,
        const NpcAttackCameraImpactDefinition& definition,
        Vector2 directionFromAttackerToPlayerWorld,
        float playerYawRadians);
void UpdatePlayerHitCamera(PlayerHitCameraState& state, float deltaSeconds);

} // namespace game
