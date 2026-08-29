#pragma once

#include <raylib.h>

#include <vector>

namespace game {

class SectorCollisionWorld;
struct SectorDynamicDoorCollider;
struct SectorStaticModelCollider;

bool HasNpcLineOfSight(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 origin,
        Vector3 target);

} // namespace game
