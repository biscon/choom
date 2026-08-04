#include "sector_demo/SectorStaticModelCollision.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/ModelAssets.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float StaticModelCollisionEpsilon = 0.0001f;

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

Vector2 Add(Vector2 a, Vector2 b)
{
    return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 Subtract(Vector2 a, Vector2 b)
{
    return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 Scale(Vector2 value, float scale)
{
    return Vector2{value.x * scale, value.y * scale};
}

float Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vector2 NormalizeOrFallback(Vector2 value, Vector2 fallback)
{
    const float lengthSquared = Dot(value, value);
    if (!(lengthSquared > StaticModelCollisionEpsilon)
            || !std::isfinite(lengthSquared)) {
        return fallback;
    }
    return Scale(value, 1.0f / std::sqrt(lengthSquared));
}

SectorCollisionMoveConfig NormalizeMoveConfig(SectorCollisionMoveConfig config)
{
    if (!std::isfinite(config.radius)) {
        config.radius = 0.25f;
    }
    if (!std::isfinite(config.playerHeight)) {
        config.playerHeight = 1.6f;
    }
    if (!std::isfinite(config.stepHeight)) {
        config.stepHeight = 0.25f;
    }
    config.radius = std::clamp(config.radius, 0.001f, 64.0f);
    config.playerHeight = std::clamp(config.playerHeight, 0.001f, 64.0f);
    config.stepHeight = std::clamp(config.stepHeight, 0.0f, 64.0f);
    config.maxIterations = std::clamp(config.maxIterations, 1, 16);
    return config;
}

bool IsValidCollider(const SectorStaticModelCollider& collider)
{
    return collider.resolved
            && !collider.failed
            && collider.placedObjectId > 0
            && IsFinite(collider.center)
            && IsFinite(collider.axisX)
            && IsFinite(collider.axisZ)
            && IsFinite(collider.halfExtents)
            && std::isfinite(collider.bottom)
            && std::isfinite(collider.top)
            && collider.halfExtents.x > StaticModelCollisionEpsilon
            && collider.halfExtents.y > StaticModelCollisionEpsilon
            && collider.top > collider.bottom + StaticModelCollisionEpsilon;
}

Vector2 ToLocalPoint(Vector2 point, const SectorStaticModelCollider& collider)
{
    const Vector2 relative = Subtract(point, collider.center);
    return Vector2{
            Dot(relative, collider.axisX),
            Dot(relative, collider.axisZ)};
}

Vector2 ColliderWorldAabbHalfExtents(
        const SectorStaticModelCollider& collider)
{
    return Vector2{
            std::fabs(collider.axisX.x) * collider.halfExtents.x
                    + std::fabs(collider.axisZ.x) * collider.halfExtents.y,
            std::fabs(collider.axisX.y) * collider.halfExtents.x
                    + std::fabs(collider.axisZ.y) * collider.halfExtents.y};
}

bool CircleMayOverlapColliderAabb(
        Vector2 position,
        float radius,
        const SectorStaticModelCollider& collider)
{
    const Vector2 half = ColliderWorldAabbHalfExtents(collider);
    return position.x + radius >= collider.center.x - half.x
            && position.x - radius <= collider.center.x + half.x
            && position.y + radius >= collider.center.y - half.y
            && position.y - radius <= collider.center.y + half.y;
}

bool SweepMayOverlapColliderAabb(
        Vector2 start,
        Vector2 delta,
        float radius,
        const SectorStaticModelCollider& collider)
{
    const Vector2 end = Add(start, delta);
    const Vector2 half = ColliderWorldAabbHalfExtents(collider);
    const float sweepMinX = std::min(start.x, end.x) - radius;
    const float sweepMaxX = std::max(start.x, end.x) + radius;
    const float sweepMinZ = std::min(start.y, end.y) - radius;
    const float sweepMaxZ = std::max(start.y, end.y) + radius;
    return sweepMaxX >= collider.center.x - half.x
            && sweepMinX <= collider.center.x + half.x
            && sweepMaxZ >= collider.center.y - half.y
            && sweepMinZ <= collider.center.y + half.y;
}

bool CircleOverlapsCollider(
        Vector2 position,
        float radius,
        const SectorStaticModelCollider& collider)
{
    if (!CircleMayOverlapColliderAabb(position, radius, collider)) {
        return false;
    }
    const Vector2 local = ToLocalPoint(position, collider);
    const Vector2 closest{
            std::clamp(local.x, -collider.halfExtents.x, collider.halfExtents.x),
            std::clamp(local.y, -collider.halfExtents.y, collider.halfExtents.y)};
    const Vector2 delta = Subtract(local, closest);
    return Dot(delta, delta)
            <= radius * radius + StaticModelCollisionEpsilon;
}

bool PlayerVerticalIntervalOverlapsCollider(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveConfig& config,
        const SectorStaticModelCollider& collider)
{
    const float playerBottom = moveState.feetY;
    const float playerTop = moveState.feetY + config.playerHeight;
    return playerTop > collider.bottom + StaticModelCollisionEpsilon
            && playerBottom < collider.top - StaticModelCollisionEpsilon;
}

enum class ColliderBlockReason {
    None,
    Side,
    Step,
    Ceiling
};

ColliderBlockReason BlockReasonForCollider(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveConfig& config,
        const SectorFpsVerticalContext& sectorContext,
        const SectorStaticModelCollider& collider)
{
    if (!PlayerVerticalIntervalOverlapsCollider(moveState, config, collider)) {
        return ColliderBlockReason::None;
    }

    if (moveState.grounded
            && collider.top > moveState.feetY + StaticModelCollisionEpsilon) {
        const float rise = collider.top - moveState.feetY;
        if (rise <= config.stepHeight + StaticModelCollisionEpsilon) {
            if (!sectorContext.hasSector
                    || collider.top + config.playerHeight
                            > sectorContext.ceilingZ + StaticModelCollisionEpsilon) {
                return ColliderBlockReason::Ceiling;
            }
            return ColliderBlockReason::None;
        }
        return ColliderBlockReason::Step;
    }
    return ColliderBlockReason::Side;
}

bool SweepCircleAgainstCollider(
        Vector2 start,
        Vector2 delta,
        float radius,
        const SectorStaticModelCollider& collider,
        float& outTime,
        Vector2& outNormal)
{
    const Vector2 startLocal = ToLocalPoint(start, collider);
    const Vector2 deltaLocal{
            Dot(delta, collider.axisX),
            Dot(delta, collider.axisZ)};
    const float movementLengthSquared = Dot(deltaLocal, deltaLocal);
    if (!(movementLengthSquared
                    > StaticModelCollisionEpsilon * StaticModelCollisionEpsilon)
            || !std::isfinite(movementLengthSquared)) {
        return false;
    }

    float earliest = std::numeric_limits<float>::infinity();
    Vector2 earliestNormalLocal{};
    const auto recordCandidate = [&](float time, Vector2 normalLocal) {
        if (!std::isfinite(time)
                || !IsFinite(normalLocal)
                || time < -StaticModelCollisionEpsilon
                || time > 1.0f + StaticModelCollisionEpsilon
                || Dot(deltaLocal, normalLocal) >= -StaticModelCollisionEpsilon) {
            return;
        }
        const float clampedTime = std::clamp(time, 0.0f, 1.0f);
        if (clampedTime < earliest) {
            earliest = clampedTime;
            earliestNormalLocal = normalLocal;
        }
    };

    const float faceX = collider.halfExtents.x + radius;
    if (std::fabs(deltaLocal.x) > StaticModelCollisionEpsilon) {
        for (float sign : {-1.0f, 1.0f}) {
            const Vector2 normalLocal{sign, 0.0f};
            const float time = (sign * faceX - startLocal.x) / deltaLocal.x;
            const float zAtHit = startLocal.y + deltaLocal.y * time;
            if (zAtHit >= -collider.halfExtents.y - StaticModelCollisionEpsilon
                    && zAtHit <= collider.halfExtents.y + StaticModelCollisionEpsilon) {
                recordCandidate(time, normalLocal);
            }
        }
    }

    const float faceZ = collider.halfExtents.y + radius;
    if (std::fabs(deltaLocal.y) > StaticModelCollisionEpsilon) {
        for (float sign : {-1.0f, 1.0f}) {
            const Vector2 normalLocal{0.0f, sign};
            const float time = (sign * faceZ - startLocal.y) / deltaLocal.y;
            const float xAtHit = startLocal.x + deltaLocal.x * time;
            if (xAtHit >= -collider.halfExtents.x - StaticModelCollisionEpsilon
                    && xAtHit <= collider.halfExtents.x + StaticModelCollisionEpsilon) {
                recordCandidate(time, normalLocal);
            }
        }
    }

    const float radiusSquared = radius * radius;
    for (float signX : {-1.0f, 1.0f}) {
        for (float signZ : {-1.0f, 1.0f}) {
            const Vector2 corner{
                    signX * collider.halfExtents.x,
                    signZ * collider.halfExtents.y};
            const Vector2 fromCorner = Subtract(startLocal, corner);
            const float projected = Dot(fromCorner, deltaLocal);
            const float constant = Dot(fromCorner, fromCorner) - radiusSquared;
            const float discriminant = projected * projected
                    - movementLengthSquared * constant;
            if (!std::isfinite(discriminant)
                    || discriminant < -StaticModelCollisionEpsilon) {
                continue;
            }

            const float time = (-projected
                    - std::sqrt(std::max(0.0f, discriminant)))
                    / movementLengthSquared;
            if (!std::isfinite(time)) {
                continue;
            }
            const Vector2 hitPoint = Add(startLocal, Scale(deltaLocal, time));
            const Vector2 fromHitCorner = Subtract(hitPoint, corner);
            if (signX * fromHitCorner.x < -StaticModelCollisionEpsilon
                    || signZ * fromHitCorner.y < -StaticModelCollisionEpsilon) {
                continue;
            }
            recordCandidate(
                    time,
                    NormalizeOrFallback(
                            fromHitCorner,
                            Vector2{signX, signZ}));
        }
    }

    if (!std::isfinite(earliest)) {
        return false;
    }

    outTime = earliest;
    outNormal = Add(
            Scale(collider.axisX, earliestNormalLocal.x),
            Scale(collider.axisZ, earliestNormalLocal.y));
    outNormal = NormalizeOrFallback(outNormal, Vector2{1.0f, 0.0f});
    return true;
}

bool ResolveCircleAgainstCollider(
        Vector2& position,
        float radius,
        const SectorStaticModelCollider& collider)
{
    const Vector2 local = ToLocalPoint(position, collider);
    const Vector2 closest{
            std::clamp(local.x, -collider.halfExtents.x, collider.halfExtents.x),
            std::clamp(local.y, -collider.halfExtents.y, collider.halfExtents.y)};
    const Vector2 delta = Subtract(local, closest);
    const float distanceSquared = Dot(delta, delta);
    const float radiusSquared = radius * radius;
    Vector2 pushLocal{};
    if (distanceSquared > StaticModelCollisionEpsilon) {
        if (distanceSquared >= radiusSquared - StaticModelCollisionEpsilon) {
            return false;
        }
        const float distance = std::sqrt(distanceSquared);
        const float penetration = radius - distance + StaticModelCollisionEpsilon;
        pushLocal = Scale(delta, penetration / distance);
    } else {
        const float overlapX = collider.halfExtents.x + radius - std::fabs(local.x);
        const float overlapZ = collider.halfExtents.y + radius - std::fabs(local.y);
        if (overlapX <= 0.0f || overlapZ <= 0.0f) {
            return false;
        }
        if (overlapX < overlapZ) {
            pushLocal.x = (local.x < 0.0f ? -1.0f : 1.0f)
                    * (overlapX + StaticModelCollisionEpsilon);
        } else {
            pushLocal.y = (local.y < 0.0f ? -1.0f : 1.0f)
                    * (overlapZ + StaticModelCollisionEpsilon);
        }
    }

    position = Add(
            position,
            Add(
                    Scale(collider.axisX, pushLocal.x),
                    Scale(collider.axisZ, pushLocal.y)));
    return true;
}

} // namespace

bool BuildSectorStaticModelCollider(
        int placedObjectId,
        BoundingBox localBounds,
        const SectorObjectTransform& transform,
        float scale,
        SectorStaticModelCollider& outCollider)
{
    outCollider = SectorStaticModelCollider{};
    outCollider.placedObjectId = placedObjectId;
    if (placedObjectId <= 0
            || !IsFinite(localBounds.min)
            || !IsFinite(localBounds.max)
            || !IsFinite(transform.position)
            || !std::isfinite(transform.yawRadians)
            || !std::isfinite(transform.rotationXRadians)
            || !std::isfinite(transform.rotationZRadians)
            || !std::isfinite(scale)
            || scale <= 0.0f) {
        outCollider.failed = true;
        return false;
    }

    const Vector3 localSize = Vector3Subtract(localBounds.max, localBounds.min);
    if (localSize.x <= StaticModelCollisionEpsilon
            || localSize.y <= StaticModelCollisionEpsilon
            || localSize.z <= StaticModelCollisionEpsilon) {
        outCollider.failed = true;
        return false;
    }

    const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
            transform.position,
            transform.rotationXRadians,
            transform.yawRadians,
            transform.rotationZRadians,
            scale);
    const float cosine = std::cos(transform.yawRadians);
    const float sine = std::sin(transform.yawRadians);
    outCollider.axisX = NormalizeOrFallback(
            Vector2{cosine, -sine},
            Vector2{1.0f, 0.0f});
    outCollider.axisZ = NormalizeOrFallback(
            Vector2{sine, cosine},
            Vector2{0.0f, 1.0f});

    float minimumX = std::numeric_limits<float>::infinity();
    float maximumX = -std::numeric_limits<float>::infinity();
    float minimumZ = std::numeric_limits<float>::infinity();
    float maximumZ = -std::numeric_limits<float>::infinity();
    float minimumY = std::numeric_limits<float>::infinity();
    float maximumY = -std::numeric_limits<float>::infinity();
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float y : {localBounds.min.y, localBounds.max.y}) {
            for (float z : {localBounds.min.z, localBounds.max.z}) {
                const Vector3 world = Vector3Transform(
                        Vector3{x, y, z},
                        authoredTransform);
                if (!IsFinite(world)) {
                    outCollider.failed = true;
                    return false;
                }
                const Vector2 horizontal{world.x, world.z};
                const float projectedX = Dot(horizontal, outCollider.axisX);
                const float projectedZ = Dot(horizontal, outCollider.axisZ);
                minimumX = std::min(minimumX, projectedX);
                maximumX = std::max(maximumX, projectedX);
                minimumZ = std::min(minimumZ, projectedZ);
                maximumZ = std::max(maximumZ, projectedZ);
                minimumY = std::min(minimumY, world.y);
                maximumY = std::max(maximumY, world.y);
            }
        }
    }

    const float centerX = (minimumX + maximumX) * 0.5f;
    const float centerZ = (minimumZ + maximumZ) * 0.5f;
    outCollider.center = Add(
            Scale(outCollider.axisX, centerX),
            Scale(outCollider.axisZ, centerZ));
    outCollider.halfExtents = Vector2{
            (maximumX - minimumX) * 0.5f,
            (maximumZ - minimumZ) * 0.5f};
    outCollider.bottom = minimumY;
    outCollider.top = maximumY;
    outCollider.resolved = true;
    outCollider.resolved = IsValidCollider(outCollider);
    outCollider.failed = !outCollider.resolved;
    return outCollider.resolved;
}

void UpdateSectorStaticModelColliderSystem(
        engine::World& world,
        engine::AssetManager& assets)
{
    world.ForEach<
            SectorObjectTransform,
            SectorStaticModel,
            SectorStaticModelCollider>(
            [&assets](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorStaticModel& staticModel,
                    SectorStaticModelCollider& collider) {
                if (collider.resolved || collider.failed) {
                    return;
                }
                if (engine::IsNull(staticModel.model)) {
                    collider.failed = true;
                    return;
                }
                const engine::ModelAsset* asset =
                        assets.GetModelAsset(staticModel.model);
                if (asset == nullptr) {
                    if (assets.HasFailed(staticModel.model)) {
                        collider.failed = true;
                    }
                    return;
                }
                if (!asset->hasLocalBounds
                        || !BuildSectorStaticModelCollider(
                                staticModel.placedObjectId,
                                asset->localBounds,
                                transform,
                                staticModel.scale,
                                collider)) {
                    collider.failed = true;
                    std::fprintf(
                            stderr,
                            "[SectorRuntimeObjects WARNING] Static model object %d has no valid collision bounds\n",
                            staticModel.placedObjectId);
                }
            });
}

void CollectSectorStaticModelColliders(
        engine::World& world,
        std::vector<SectorStaticModelCollider>& colliders)
{
    colliders.clear();
    world.ForEach<SectorStaticModelCollider>(
            [&colliders](engine::Entity, SectorStaticModelCollider& collider) {
                if (IsValidCollider(collider)) {
                    colliders.push_back(collider);
                }
            });
}

SectorCollisionMoveResult ResolveSectorStaticModelCollidersForPlayerMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& sectorAndDoorResult,
        const SectorCollisionMoveConfig& moveConfig,
        const SectorFpsVerticalContext& sectorContext,
        const std::vector<SectorStaticModelCollider>& colliders)
{
    SectorCollisionMoveResult result = sectorAndDoorResult;
    const SectorCollisionMoveConfig config = NormalizeMoveConfig(moveConfig);
    if (!IsFinite(moveState.positionXZ)
            || !IsFinite(result.positionXZ)
            || colliders.empty()) {
        return result;
    }

    Vector2 position = moveState.positionXZ;
    Vector2 remaining = Subtract(result.positionXZ, moveState.positionXZ);
    const auto markCollision = [&result](ColliderBlockReason reason) {
        result.hitWall = true;
        result.blockedByStep = result.blockedByStep
                || reason == ColliderBlockReason::Step;
        result.blockedByCeiling = result.blockedByCeiling
                || reason == ColliderBlockReason::Ceiling;
    };
    const auto resolvePenetrations = [&]() {
        for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
            bool changed = false;
            for (const SectorStaticModelCollider& collider : colliders) {
                if (!IsValidCollider(collider)) {
                    continue;
                }
                const ColliderBlockReason reason = BlockReasonForCollider(
                        moveState,
                        config,
                        sectorContext,
                        collider);
                if (reason == ColliderBlockReason::None
                        || !CircleMayOverlapColliderAabb(
                                position,
                                config.radius,
                                collider)) {
                    continue;
                }
                if (ResolveCircleAgainstCollider(position, config.radius, collider)) {
                    markCollision(reason);
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
    };

    // Recover first so a frame that begins slightly embedded cannot turn that
    // overlap into a persistent zero-time sweep contact.
    resolvePenetrations();
    for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
        float earliest = std::numeric_limits<float>::infinity();
        Vector2 hitNormal{};
        ColliderBlockReason hitReason = ColliderBlockReason::None;
        for (const SectorStaticModelCollider& collider : colliders) {
            if (!IsValidCollider(collider)) {
                continue;
            }
            const ColliderBlockReason reason = BlockReasonForCollider(
                    moveState,
                    config,
                    sectorContext,
                    collider);
            if (reason == ColliderBlockReason::None) {
                continue;
            }
            if (!SweepMayOverlapColliderAabb(
                        position,
                        remaining,
                        config.radius,
                        collider)) {
                continue;
            }
            float hitTime = 0.0f;
            Vector2 normal{};
            if (SweepCircleAgainstCollider(
                        position,
                        remaining,
                        config.radius,
                        collider,
                        hitTime,
                        normal)
                    && hitTime < earliest) {
                earliest = hitTime;
                hitNormal = normal;
                hitReason = reason;
            }
        }

        if (!std::isfinite(earliest)) {
            position = Add(position, remaining);
            remaining = Vector2{};
            break;
        }

        const float approachDistance = -Dot(remaining, hitNormal);
        const float skinTime = approachDistance > StaticModelCollisionEpsilon
                ? StaticModelCollisionEpsilon / approachDistance
                : 0.0f;
        const float safeTime = std::max(0.0f, earliest - skinTime);
        position = Add(position, Scale(remaining, safeTime));
        remaining = Scale(remaining, 1.0f - earliest);
        const float intoSurface = Dot(remaining, hitNormal);
        if (intoSurface < 0.0f) {
            remaining = Subtract(remaining, Scale(hitNormal, intoSurface));
        }
        markCollision(hitReason);
        if (Dot(remaining, remaining)
                <= StaticModelCollisionEpsilon * StaticModelCollisionEpsilon) {
            remaining = Vector2{};
            break;
        }
    }

    resolvePenetrations();

    result.positionXZ = position;
    return result;
}

SectorFpsVerticalContext BuildSectorStaticModelVerticalContext(
        const SectorFpsVerticalContext& sectorContext,
        const SectorFpsControllerState& playerState,
        const SectorFpsControllerConfig& playerConfig,
        const std::vector<SectorStaticModelCollider>& colliders)
{
    SectorFpsVerticalContext result = sectorContext;
    if (!result.hasSector || colliders.empty()) {
        return result;
    }

    const SectorFpsControllerConfig config =
            NormalizeSectorFpsControllerConfig(playerConfig);
    const Vector2 playerXZ{
            playerState.feetPosition.x,
            playerState.feetPosition.z};
    const float reachableFloor = playerState.feetPosition.y
            + (playerState.grounded ? config.stepHeight : 0.0f)
            + StaticModelCollisionEpsilon;
    const float playerTop = playerState.feetPosition.y + config.playerHeight;
    for (const SectorStaticModelCollider& collider : colliders) {
        if (!IsValidCollider(collider)
                || !CircleOverlapsCollider(
                        playerXZ,
                        config.playerRadius,
                        collider)) {
            continue;
        }
        if (collider.top <= reachableFloor
                && collider.top > result.floorZ) {
            result.floorZ = collider.top;
        }
        if (collider.bottom >= playerTop - StaticModelCollisionEpsilon
                && collider.bottom < result.ceilingZ) {
            result.ceilingZ = collider.bottom;
        }
    }
    return result;
}

} // namespace game
