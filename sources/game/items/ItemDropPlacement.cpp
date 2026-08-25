#include "game/items/ItemDropPlacement.h"

#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace game {
namespace {

bool Finite(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z);
}

BoundingBox ColliderBounds(
        Vector2 center,
        Vector2 axisX,
        Vector2 axisZ,
        Vector2 halfExtents,
        float bottom,
        float top)
{
    const float extentX = std::fabs(axisX.x) * halfExtents.x
            + std::fabs(axisZ.x) * halfExtents.y;
    const float extentZ = std::fabs(axisX.y) * halfExtents.x
            + std::fabs(axisZ.y) * halfExtents.y;
    return BoundingBox{
            Vector3{center.x - extentX, bottom, center.y - extentZ},
            Vector3{center.x + extentX, top, center.y + extentZ}};
}

} // namespace

BoundingBox TransformItemDropBounds(
        BoundingBox localBounds,
        Matrix transform)
{
    BoundingBox result{
            Vector3{FLT_MAX, FLT_MAX, FLT_MAX},
            Vector3{-FLT_MAX, -FLT_MAX, -FLT_MAX}};
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float y : {localBounds.min.y, localBounds.max.y}) {
            for (float z : {localBounds.min.z, localBounds.max.z}) {
                const Vector3 point = Vector3Transform(
                        Vector3{x, y, z}, transform);
                result.min.x = std::min(result.min.x, point.x);
                result.min.y = std::min(result.min.y, point.y);
                result.min.z = std::min(result.min.z, point.z);
                result.max.x = std::max(result.max.x, point.x);
                result.max.y = std::max(result.max.y, point.y);
                result.max.z = std::max(result.max.z, point.z);
            }
        }
    }
    return result;
}

ItemDropCandidate BuildItemDropCandidate(
        const SectorCollisionWorld& collisionWorld,
        int currentSectorId,
        Vector3 desiredOriginXZ,
        BoundingBox localBounds)
{
    ItemDropCandidate candidate;
    if (!Finite(desiredOriginXZ) || !Finite(localBounds.min)
            || !Finite(localBounds.max)
            || localBounds.max.x <= localBounds.min.x
            || localBounds.max.y <= localBounds.min.y
            || localBounds.max.z <= localBounds.min.z) {
        return candidate;
    }
    const int sectorId = collisionWorld.FindSectorContainingPointPreferCurrent(
            Vector2{desiredOriginXZ.x, desiredOriginXZ.z}, currentSectorId);
    SectorCollisionHeights heights;
    if (sectorId == 0
            || !collisionWorld.GetSectorFloorCeiling(sectorId, &heights)) {
        return candidate;
    }
    candidate.originWorld = Vector3{
            desiredOriginXZ.x,
            heights.floorZ - localBounds.min.y,
            desiredOriginXZ.z};
    candidate.worldBounds = TransformItemDropBounds(
            localBounds, MatrixTranslate(
                    candidate.originWorld.x,
                    candidate.originWorld.y,
                    candidate.originWorld.z));
    candidate.sectorId = sectorId;
    candidate.valid = Finite(candidate.worldBounds.min)
            && Finite(candidate.worldBounds.max);
    return candidate;
}

bool ItemDropBoundsOverlap(BoundingBox first, BoundingBox second)
{
    return first.max.x > second.min.x && first.min.x < second.max.x
            && first.max.y > second.min.y && first.min.y < second.max.y
            && first.max.z > second.min.z && first.min.z < second.max.z;
}

bool ItemDropBoundsOverlap(
        BoundingBox candidate,
        const SectorStaticModelCollider& obstacle)
{
    if (!obstacle.resolved || obstacle.failed) return false;
    return ItemDropBoundsOverlap(
            candidate,
            ColliderBounds(
                    obstacle.center, obstacle.axisX, obstacle.axisZ,
                    obstacle.halfExtents, obstacle.bottom, obstacle.top));
}

bool ItemDropBoundsOverlap(
        BoundingBox candidate,
        const SectorDynamicDoorCollider& obstacle)
{
    return ItemDropBoundsOverlap(
            candidate,
            ColliderBounds(
                    obstacle.center, obstacle.tangent, obstacle.normal,
                    obstacle.halfExtents, obstacle.bottom, obstacle.top));
}

bool ItemDropBoundsOverlapPlayer(
        BoundingBox candidate,
        Vector3 feetPosition,
        float radius,
        float height)
{
    if (!Finite(feetPosition) || !std::isfinite(radius)
            || !std::isfinite(height) || radius <= 0.0f || height <= 0.0f
            || candidate.max.y <= feetPosition.y
            || candidate.min.y >= feetPosition.y + height) {
        return false;
    }
    const float closestX = std::clamp(
            feetPosition.x, candidate.min.x, candidate.max.x);
    const float closestZ = std::clamp(
            feetPosition.z, candidate.min.z, candidate.max.z);
    const float dx = feetPosition.x - closestX;
    const float dz = feetPosition.z - closestZ;
    return dx * dx + dz * dz < radius * radius;
}

bool ItemDropFitsTopology(
        const SectorCollisionWorld& collisionWorld,
        const ItemDropCandidate& candidate)
{
    if (!candidate.valid) return false;
    const Vector2 center{
            (candidate.worldBounds.min.x + candidate.worldBounds.max.x) * 0.5f,
            (candidate.worldBounds.min.z + candidate.worldBounds.max.z) * 0.5f};
    const float halfX = (candidate.worldBounds.max.x
            - candidate.worldBounds.min.x) * 0.5f;
    const float halfZ = (candidate.worldBounds.max.z
            - candidate.worldBounds.min.z) * 0.5f;
    const float radius = std::sqrt(halfX * halfX + halfZ * halfZ);
    return collisionWorld.AllowsPrismPlacement(
            center,
            std::max(0.001f, radius),
            candidate.worldBounds.min.y,
            candidate.worldBounds.max.y,
            candidate.sectorId,
            nullptr);
}

} // namespace game
