#pragma once

#include "game/npc/NpcRuntime.h"

#include <raylib.h>

#include <string_view>
#include <vector>

namespace engine {
class AssetManager;
class World;
struct AnimatedModelAnimator;
}

namespace game {

class SectorCollisionWorld;
class SectorNavigationWorld;
struct SectorBakedObjectLightProbeRuntimeData;
struct SectorDynamicDoorCollider;
struct SectorDoorPlayerObstacle;
struct SectorStaticModelCollider;
struct SectorTopologyMap;

void PrepareNpcDoorTraversalAndHoldsSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        float dt);

void SynchronizeSectorNavigationDoorLinksSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const std::vector<SectorDynamicDoorCollider>& doorColliders);

void CollectNpcDoorObstacles(
        engine::World& world,
        const NpcNavigationRuntime& runtime,
        std::vector<SectorDoorPlayerObstacle>& outObstacles,
        const SectorDoorPlayerObstacle* playerObstacle = nullptr);

enum class NpcAnimationApplyResult : uint8_t {
    Unchanged,
    Applied,
    Queued,
    Missing
};

NpcAnimationApplyResult ApplyNpcSemanticAnimation(
        NpcAnimationState& state,
        engine::AnimatedModelAnimator& animator,
        NpcAction requested);

void InitializeNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime);

void ShutdownNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime);

NpcMoveRequestResult RequestNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait = NpcMoveGait::Walk,
        NpcMoveAuthority authority = NpcMoveAuthority::Programmatic);

bool CancelNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        uint64_t expectedRequestId = 0);

NpcMoveStatus GetNpcMoveStatus(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId);

void UpdateNpcNavigationAndLocomotionSystem(
        engine::World& world,
        engine::AssetManager& assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        const NpcDefinitionCatalog& definitions,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap& map,
        float dt,
        const SectorDoorPlayerObstacle* playerObstacle = nullptr);

} // namespace game
