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

std::array<Vector3, kItemDropPlacementSlotCount> BuildItemDropSlotOrigins(
        Vector3 feetPosition,
        Vector3 forwardXZ,
        float playerRadius,
        BoundingBox localBounds)
{
    std::array<Vector3, kItemDropPlacementSlotCount> origins{};
    if (!Finite(feetPosition) || !Finite(forwardXZ)
            || !Finite(localBounds.min) || !Finite(localBounds.max)
            || !std::isfinite(playerRadius) || playerRadius <= 0.0f) {
        return origins;
    }
    forwardXZ.y = 0.0f;
    const float forwardLength = std::sqrt(
            forwardXZ.x * forwardXZ.x + forwardXZ.z * forwardXZ.z);
    if (!(forwardLength > 0.0001f)) return origins;
    forwardXZ.x /= forwardLength;
    forwardXZ.z /= forwardLength;
    const Vector3 right{-forwardXZ.z, 0.0f, forwardXZ.x};

    float footprintRadiusSquared = 0.0f;
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float z : {localBounds.min.z, localBounds.max.z}) {
            footprintRadiusSquared = std::max(
                    footprintRadiusSquared, x * x + z * z);
        }
    }
    const float footprintRadius = std::sqrt(footprintRadiusSquared);
    constexpr float Clearance = 0.12f;
    const float nearDistance = std::max(
            0.9f, playerRadius + footprintRadius + Clearance);
    const float spacing = std::max(
            0.65f, footprintRadius * 2.0f + Clearance);
    const std::array<Vector2, kItemDropPlacementSlotCount> fanOffsets{{
            Vector2{0.0f, nearDistance},
            Vector2{-spacing, nearDistance + spacing * 0.4f},
            Vector2{spacing, nearDistance + spacing * 0.4f},
            Vector2{0.0f, nearDistance + spacing},
            Vector2{-spacing, nearDistance + spacing * 1.4f},
            Vector2{spacing, nearDistance + spacing * 1.4f}}};
    for (std::size_t index = 0; index < origins.size(); ++index) {
        origins[index] = Vector3{
                feetPosition.x
                        + right.x * fanOffsets[index].x
                        + forwardXZ.x * fanOffsets[index].y,
                0.0f,
                feetPosition.z
                        + right.z * fanOffsets[index].x
                        + forwardXZ.z * fanOffsets[index].y};
    }
    return origins;
}

float BuildItemDropRandomYawRadians(
        std::uint64_t itemRuntimeId,
        std::uint64_t droppedObjectId)
{
    std::uint64_t value = itemRuntimeId
            ^ (droppedObjectId + 0x9e3779b97f4a7c15ull
                    + (itemRuntimeId << 6u) + (itemRuntimeId >> 2u));
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31u;
    const float unit = static_cast<float>((value >> 40u) & 0x00ffffffu)
            / static_cast<float>(0x01000000u);
    return unit * 2.0f * PI;
}

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
        BoundingBox localBounds,
        float yawRadians)
{
    ItemDropCandidate candidate;
    if (!Finite(desiredOriginXZ) || !Finite(localBounds.min)
            || !Finite(localBounds.max)
            || !std::isfinite(yawRadians)
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
            localBounds,
            MatrixMultiply(
                    MatrixRotateY(yawRadians),
                    MatrixTranslate(
                            candidate.originWorld.x,
                            candidate.originWorld.y,
                            candidate.originWorld.z)));
    candidate.sectorId = sectorId;
    candidate.yawRadians = yawRadians;
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

bool ItemDropBoundsOverlapAnyPropCollider(
        BoundingBox candidate,
        const std::vector<SectorStaticModelCollider>& obstacles)
{
    for (const SectorStaticModelCollider& obstacle : obstacles) {
        if (ItemDropBoundsOverlap(candidate, obstacle)) return true;
    }
    return false;
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
