#pragma once

#include "sector_demo/SectorCollisionWorld.h"

#include <raylib.h>

#include <cstddef>

namespace game {

struct NpcCollisionCylinder {
    int stableId = 0;
    Vector3 feetPosition = {};
    float radius = 0.25f;
    float height = 1.6f;
};

SectorCollisionMoveResult ResolveNpcCollisionCylindersForMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& resolvedMovement,
        const SectorCollisionMoveConfig& moveConfig,
        int movingStableId,
        const NpcCollisionCylinder* obstacles,
        size_t obstacleCount,
        bool* outBlocked = nullptr);

} // namespace game
