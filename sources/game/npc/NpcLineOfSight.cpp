#include "game/npc/NpcLineOfSight.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorStaticModelCollision.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float LineOfSightEpsilon = 0.03f;

bool SegmentIntersectsPrism(
        Vector3 origin,
        Vector3 direction,
        float maximumDistance,
        Vector2 center,
        Vector2 axisX,
        Vector2 axisZ,
        Vector2 halfExtents,
        float bottom,
        float top)
{
    const Vector2 relative{origin.x - center.x, origin.z - center.y};
    const float localOrigin[3] = {
            Vector2DotProduct(relative, axisX),
            origin.y,
            Vector2DotProduct(relative, axisZ)};
    const float localDirection[3] = {
            direction.x * axisX.x + direction.z * axisX.y,
            direction.y,
            direction.x * axisZ.x + direction.z * axisZ.y};
    const float minimum[3] = {-halfExtents.x, bottom, -halfExtents.y};
    const float maximum[3] = {halfExtents.x, top, halfExtents.y};
    float enter = 0.0f;
    float leave = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(localDirection[axis]) <= 0.000001f) {
            if (localOrigin[axis] < minimum[axis]
                    || localOrigin[axis] > maximum[axis]) return false;
            continue;
        }
        float a = (minimum[axis] - localOrigin[axis]) / localDirection[axis];
        float b = (maximum[axis] - localOrigin[axis]) / localDirection[axis];
        if (a > b) std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        if (enter > leave) return false;
    }
    return enter >= 0.0f && enter < maximumDistance - LineOfSightEpsilon;
}

} // namespace

bool HasNpcLineOfSight(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 origin,
        Vector3 target)
{
    const Vector3 delta = Vector3Subtract(target, origin);
    const float distance = Vector3Length(delta);
    if (distance <= LineOfSightEpsilon) return true;
    const Vector3 direction = Vector3Scale(delta, 1.0f / distance);
    const SectorCollisionRayHit sectorHit = collisionWorld.Raycast(
            origin, direction, distance);
    if (sectorHit.hit && sectorHit.distance < distance - LineOfSightEpsilon) {
        return false;
    }
    for (const SectorDynamicDoorCollider& door : doorColliders) {
        if (SegmentIntersectsPrism(
                    origin, direction, distance,
                    door.center, door.tangent, door.normal,
                    door.halfExtents, door.bottom, door.top)) return false;
    }
    for (const SectorStaticModelCollider& model : staticColliders) {
        if (!model.resolved || model.failed) continue;
        if (SegmentIntersectsPrism(
                    origin, direction, distance,
                    model.center, model.axisX, model.axisZ,
                    model.halfExtents, model.bottom, model.top)) return false;
    }
    return true;
}

} // namespace game
