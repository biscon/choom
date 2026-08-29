#pragma once

#include "game/npc/NpcDefinitions.h"

#include <raylib.h>

#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorCollisionWorld;
struct SectorDynamicDoorCollider;
struct SectorStaticModelCollider;

inline constexpr float kNpcHeadLookTurnSpeedDegreesPerSecond = 180.0f;

struct NpcHeadLookTargetAngles {
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    bool active = false;
};

NpcHeadLookTargetAngles EvaluateNpcHeadLookTargetAngles(
        Vector3 npcPosition,
        float npcYawRadians,
        Vector3 headWorldPosition,
        Vector3 playerEyePosition,
        const NpcHeadLookDefinition& definition,
        bool hasLineOfSight);

float MoveNpcHeadLookAngleToward(
        float currentRadians,
        float targetRadians,
        float deltaSeconds);

void UpdateNpcHeadLookSystem(
        engine::World& world,
        engine::AssetManager& assets,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const Vector3* playerEyePosition,
        float deltaSeconds);

} // namespace game
