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
        float dt,
        bool freezeAi = false);

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

void UpdateNpcAnimationStateSystem(
        engine::World& world,
        engine::AssetManager& assets,
        const NpcDefinitionCatalog& definitions);

void InitializeNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime);

void ShutdownNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime);

bool DeactivateNpcNavigation(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        engine::Entity entity);

NpcMoveRequestResult RequestNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait = NpcMoveGait::Walk,
        NpcMoveAuthority authority = NpcMoveAuthority::Programmatic);

// Replaces an AI-owned route only after its new path has been found. A failed
// retarget leaves the previous route, door holds, and request ID intact.
NpcMoveRequestResult RetargetNpcAiMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait = NpcMoveGait::Run);

bool CancelNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        uint64_t expectedRequestId = 0);

NpcMoveStatus GetNpcMoveStatus(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId);

bool UpdateNpcFootstepCadence(
        NpcNavigationRecord& record,
        bool active,
        float resolvedHorizontalDistance);

void ResetNpcWaypointProgressTracking(NpcNavigationRecord& record);

void UpdateNpcWaypointProgressTracking(
        NpcNavigationRecord& record,
        size_t cornerIndex,
        float cornerDistance,
        float agentRadius,
        float maximumSpeed,
        float dt);

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
        const SectorDoorPlayerObstacle* playerObstacle = nullptr,
        bool freezeAi = false);

} // namespace game
