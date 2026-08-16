#include "sector_demo/SectorAudioOcclusion.h"

#include "sector_demo/SectorCollisionWorld.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float AudioOcclusionEpsilon = 0.001f;
constexpr float AudioOcclusionEndpointEpsilon = 0.01f;

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

bool PointInsideDoorCollider(
        Vector3 point,
        const SectorDynamicDoorCollider& door)
{
    const Vector2 offset{point.x - door.center.x, point.z - door.center.y};
    const float localX = Vector2DotProduct(offset, door.tangent);
    const float localZ = Vector2DotProduct(offset, door.normal);
    return std::fabs(localX) <= door.halfExtents.x + AudioOcclusionEpsilon
            && std::fabs(localZ)
                    <= door.halfExtents.y + AudioOcclusionEpsilon
            && point.y >= door.bottom - AudioOcclusionEpsilon
            && point.y <= door.top + AudioOcclusionEpsilon;
}

bool SegmentIntersectsDoorCollider(
        Vector3 origin,
        Vector3 endpoint,
        Vector3 direction,
        float maximumDistance,
        const SectorDynamicDoorCollider& door)
{
    if (PointInsideDoorCollider(origin, door)
            || PointInsideDoorCollider(endpoint, door)) {
        return false;
    }
    const Vector2 offset{origin.x - door.center.x, origin.z - door.center.y};
    const Vector2 directionXZ{direction.x, direction.z};
    const float localOrigin[3] = {
            Vector2DotProduct(offset, door.tangent),
            origin.y - (door.bottom + door.top) * 0.5f,
            Vector2DotProduct(offset, door.normal)};
    const float localDirection[3] = {
            Vector2DotProduct(directionXZ, door.tangent),
            direction.y,
            Vector2DotProduct(directionXZ, door.normal)};
    const float extents[3] = {
            std::max(0.0f, door.halfExtents.x),
            std::max(0.0f, (door.top - door.bottom) * 0.5f),
            std::max(0.0f, door.halfExtents.y)};
    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(localDirection[axis]) <= AudioOcclusionEpsilon) {
            if (localOrigin[axis] < -extents[axis]
                    || localOrigin[axis] > extents[axis]) {
                return false;
            }
            continue;
        }
        float first = (-extents[axis] - localOrigin[axis])
                / localDirection[axis];
        float second = (extents[axis] - localOrigin[axis])
                / localDirection[axis];
        if (first > second) std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) return false;
    }
    return nearDistance >= 0.0f && nearDistance <= maximumDistance;
}

} // namespace

float ComputeSectorSoundOcclusion(
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    if (!IsFinite(listenerPosition) || !IsFinite(sourcePosition)) return 1.0f;
    const Vector3 offset = Vector3Subtract(sourcePosition, listenerPosition);
    const float distance = Vector3Length(offset);
    if (distance <= AudioOcclusionEndpointEpsilon) return 1.0f;
    const Vector3 direction = Vector3Scale(offset, 1.0f / distance);
    const float queryDistance = distance - AudioOcclusionEndpointEpsilon;

    if (collisionWorld != nullptr
            && collisionWorld->Raycast(
                    listenerPosition, direction, queryDistance).hit) {
        return SectorOccludedSoundVolumeScale;
    }
    for (const SectorDynamicDoorCollider& door : doorColliders) {
        if (SegmentIntersectsDoorCollider(
                    listenerPosition,
                    sourcePosition,
                    direction,
                    queryDistance,
                    door)) {
            return SectorOccludedSoundVolumeScale;
        }
    }
    return 1.0f;
}

float QuerySectorSoundOcclusion(
        void* rawContext,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    if (rawContext == nullptr) return 1.0f;
    const SectorAudioOcclusionContext& context =
            *static_cast<const SectorAudioOcclusionContext*>(rawContext);
    static const std::vector<SectorDynamicDoorCollider> noDoors;
    return ComputeSectorSoundOcclusion(
            context.collisionWorld,
            context.doorColliders != nullptr
                    ? *context.doorColliders
                    : noDoors,
            listenerPosition,
            sourcePosition);
}

} // namespace game
