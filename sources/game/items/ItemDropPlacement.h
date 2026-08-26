#pragma once

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorStaticModelCollision.h"

#include <raylib.h>

#include <vector>

namespace game {

inline constexpr BoundingBox kItemDropFallbackLocalBounds{
        Vector3{-0.5f, 0.0f, -0.5f},
        Vector3{0.5f, 1.0f, 0.5f}};

struct ItemDropCandidate {
    bool valid = false;
    int sectorId = 0;
    Vector3 originWorld = {};
    BoundingBox worldBounds = {};
};

ItemDropCandidate BuildItemDropCandidate(
        const SectorCollisionWorld& collisionWorld,
        int currentSectorId,
        Vector3 desiredOriginXZ,
        BoundingBox localBounds);
BoundingBox TransformItemDropBounds(
        BoundingBox localBounds,
        Matrix transform);
bool ItemDropBoundsOverlap(BoundingBox first, BoundingBox second);
bool ItemDropBoundsOverlap(
        BoundingBox candidate,
        const SectorStaticModelCollider& obstacle);
bool ItemDropBoundsOverlap(
        BoundingBox candidate,
        const SectorDynamicDoorCollider& obstacle);
bool ItemDropBoundsOverlapAnyPropCollider(
        BoundingBox candidate,
        const std::vector<SectorStaticModelCollider>& obstacles);
bool ItemDropBoundsOverlapPlayer(
        BoundingBox candidate,
        Vector3 feetPosition,
        float radius,
        float height);
bool ItemDropFitsTopology(
        const SectorCollisionWorld& collisionWorld,
        const ItemDropCandidate& candidate);

} // namespace game
