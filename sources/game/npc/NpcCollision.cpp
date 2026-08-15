#include "game/npc/NpcCollision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {
namespace {

constexpr float CollisionEpsilon = 0.0001f;

Vector2 Add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
Vector2 Subtract(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
Vector2 Scale(Vector2 value, float scale) { return {value.x * scale, value.y * scale}; }
float Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool ValidObstacle(const NpcCollisionCylinder& obstacle)
{
    return obstacle.stableId != 0
            && std::isfinite(obstacle.feetPosition.x)
            && std::isfinite(obstacle.feetPosition.y)
            && std::isfinite(obstacle.feetPosition.z)
            && std::isfinite(obstacle.radius)
            && std::isfinite(obstacle.height)
            && obstacle.radius > CollisionEpsilon
            && obstacle.height > CollisionEpsilon;
}

bool VerticalOverlap(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveConfig& config,
        const NpcCollisionCylinder& obstacle)
{
    const float movingTop = moveState.feetY + config.playerHeight;
    const float obstacleTop = obstacle.feetPosition.y + obstacle.height;
    return movingTop > obstacle.feetPosition.y + CollisionEpsilon
            && moveState.feetY < obstacleTop - CollisionEpsilon;
}

Vector2 FallbackNormal(int movingStableId, int obstacleStableId, Vector2 movement)
{
    const float movementLengthSquared = Dot(movement, movement);
    if (movementLengthSquared > CollisionEpsilon * CollisionEpsilon) {
        const float inverse = 1.0f / std::sqrt(movementLengthSquared);
        return {-movement.x * inverse, -movement.y * inverse};
    }
    return movingStableId < obstacleStableId
            ? Vector2{-1.0f, 0.0f} : Vector2{1.0f, 0.0f};
}

} // namespace

SectorCollisionMoveResult ResolveNpcCollisionCylindersForMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& resolvedMovement,
        const SectorCollisionMoveConfig& rawConfig,
        int movingStableId,
        const NpcCollisionCylinder* obstacles,
        size_t obstacleCount,
        bool* outBlocked)
{
    SectorCollisionMoveResult result = resolvedMovement;
    if (outBlocked != nullptr) *outBlocked = false;
    if (obstacles == nullptr || obstacleCount == 0
            || !IsFinite(moveState.positionXZ)
            || !IsFinite(result.positionXZ)) {
        return result;
    }
    SectorCollisionMoveConfig config = rawConfig;
    config.radius = std::clamp(
            std::isfinite(config.radius) ? config.radius : 0.25f,
            0.001f, 64.0f);
    config.playerHeight = std::clamp(
            std::isfinite(config.playerHeight) ? config.playerHeight : 1.6f,
            0.001f, 64.0f);
    config.maxIterations = std::clamp(config.maxIterations, 1, 16);

    Vector2 position = moveState.positionXZ;
    Vector2 remaining = Subtract(result.positionXZ, moveState.positionXZ);
    bool blocked = false;

    const auto resolvePenetrations = [&]() {
        for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
            bool changed = false;
            for (size_t index = 0; index < obstacleCount; ++index) {
                const NpcCollisionCylinder& obstacle = obstacles[index];
                if (!ValidObstacle(obstacle)
                        || obstacle.stableId == movingStableId
                        || !VerticalOverlap(moveState, config, obstacle)) {
                    continue;
                }
                const Vector2 center{obstacle.feetPosition.x, obstacle.feetPosition.z};
                const Vector2 separation = Subtract(position, center);
                const float minimumDistance = config.radius + obstacle.radius;
                const float distanceSquared = Dot(separation, separation);
                if (distanceSquared >= minimumDistance * minimumDistance
                        - CollisionEpsilon) {
                    continue;
                }
                Vector2 normal;
                if (distanceSquared > CollisionEpsilon * CollisionEpsilon) {
                    normal = Scale(separation, 1.0f / std::sqrt(distanceSquared));
                } else {
                    normal = FallbackNormal(
                            movingStableId, obstacle.stableId, remaining);
                }
                const float distance = std::sqrt(std::max(0.0f, distanceSquared));
                position = Add(position, Scale(
                        normal, minimumDistance - distance + CollisionEpsilon));
                blocked = true;
                changed = true;
            }
            if (!changed) break;
        }
    };

    resolvePenetrations();
    for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
        const float movementLengthSquared = Dot(remaining, remaining);
        if (movementLengthSquared <= CollisionEpsilon * CollisionEpsilon) break;
        float earliest = std::numeric_limits<float>::infinity();
        Vector2 hitNormal{};
        for (size_t index = 0; index < obstacleCount; ++index) {
            const NpcCollisionCylinder& obstacle = obstacles[index];
            if (!ValidObstacle(obstacle)
                    || obstacle.stableId == movingStableId
                    || !VerticalOverlap(moveState, config, obstacle)) {
                continue;
            }
            const Vector2 center{obstacle.feetPosition.x, obstacle.feetPosition.z};
            const Vector2 offset = Subtract(position, center);
            const float radius = config.radius + obstacle.radius;
            const float b = Dot(offset, remaining);
            const float c = Dot(offset, offset) - radius * radius;
            if (c <= 0.0f || b >= 0.0f) continue;
            const float discriminant = b * b - movementLengthSquared * c;
            if (discriminant < 0.0f) continue;
            const float time = (-b - std::sqrt(discriminant)) / movementLengthSquared;
            if (time < 0.0f || time > 1.0f || time >= earliest) continue;
            const Vector2 contact = Add(position, Scale(remaining, time));
            const Vector2 normalDelta = Subtract(contact, center);
            const float normalLengthSquared = Dot(normalDelta, normalDelta);
            hitNormal = normalLengthSquared > CollisionEpsilon * CollisionEpsilon
                    ? Scale(normalDelta, 1.0f / std::sqrt(normalLengthSquared))
                    : FallbackNormal(movingStableId, obstacle.stableId, remaining);
            earliest = time;
        }
        if (!std::isfinite(earliest)) {
            position = Add(position, remaining);
            remaining = {};
            break;
        }
        const float approach = std::max(0.0f, -Dot(remaining, hitNormal));
        const float skinTime = approach > CollisionEpsilon
                ? CollisionEpsilon / approach : 0.0f;
        position = Add(position, Scale(remaining,
                std::max(0.0f, earliest - skinTime)));
        remaining = Scale(remaining, 1.0f - earliest);
        const float inward = Dot(remaining, hitNormal);
        if (inward < 0.0f) {
            remaining = Subtract(remaining, Scale(hitNormal, inward));
        }
        blocked = true;
    }
    resolvePenetrations();
    result.positionXZ = position;
    result.hitWall = result.hitWall || blocked;
    if (outBlocked != nullptr) *outBlocked = blocked;
    return result;
}

} // namespace game
