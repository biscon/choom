#pragma once

#include "game/npc/NpcRuntime.h"

#include <raylib.h>

#include <string_view>
#include <vector>

namespace engine {
class AssetManager;
class World;
struct AnimatedModelAnimator;
struct ModelAsset;
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
        bool freezeAi = false,
        int externalDoorHoldId = 0);

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

// Script playback uses resolved skeletal clip indices, never per-frame names.
struct NpcScriptAnimationClip {
    uint32_t index = UINT32_MAX;
    float speed = 1.0f;
    float durationSeconds = 0.0f;
};

// playbackValue is loop speed, or one-shot milliseconds (zero means native).
bool ResolveNpcScriptAnimationClip(
        const engine::ModelAsset& asset,
        const char* name,
        bool loop,
        double playbackValue,
        NpcScriptAnimationClip& clip,
        std::string& error);

void SetNpcScriptAnimation(
        NpcAnimationState& state,
        engine::AnimatedModelAnimator& animator,
        uint32_t animationIndex,
        float speed,
        bool loop,
        float durationSeconds,
        uint64_t requestId);
void ClearNpcScriptAnimation(
        NpcAnimationState& state,
        NpcScriptAnimationCancelReason reason);
void CancelNpcScriptAnimation(
        NpcAnimationState& state,
        engine::AnimatedModelAnimator& animator);
bool HasNpcScriptAnimationOverride(const NpcAnimationState& state);
bool UpdateNpcScriptAnimation(
        NpcAnimationState& state,
        engine::AnimatedModelAnimator& animator);
void UpdateNpcAnimationState(
        engine::World& world,
        engine::AssetManager& assets,
        const NpcDefinitionCatalog& definitions,
        engine::Entity entity);

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

NpcBodyTurnState* FindNpcBodyTurn(NpcNavigationRuntime& runtime, engine::Entity entity);
bool HasNpcBodyTurn(const NpcNavigationRuntime& runtime, engine::Entity entity);
bool BeginNpcBodyTurn(
        engine::World& world, NpcNavigationRuntime& runtime, engine::Entity entity,
        NpcBodyTurnState turn, const Vector3* playerPosition, std::string& error);
// reason must have static lifetime; the runtime retains it until operation resolution.
void CancelNpcBodyTurn(NpcNavigationRuntime& runtime, engine::Entity entity,
        uint64_t requestId, const char* reason = "NPC look was cancelled");
// Shared shortest-angle easing for arrival orientation and standalone looks.
bool AdvanceNpcBodyTurn(NpcBodyTurnState& turn, float& yaw, float dt);
void UpdateNpcArrivalTurn(NpcNavigationRecord& record, float& yaw, float dt);

NpcMoveRequestResult RequestNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait = NpcMoveGait::Walk,
        NpcMoveAuthority authority = NpcMoveAuthority::Programmatic,
        float movementSpeedOverride = 0.0f,
        const float* arrivalYaw = nullptr);

NpcMoveRequestResult RequestNpcMoveForEntity(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        engine::Entity entity,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority,
        float movementSpeedOverride = 0.0f,
        const float* arrivalYaw = nullptr);

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

bool CancelNpcMoveForEntity(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        engine::Entity entity,
        uint64_t expectedRequestId = 0);

NpcMoveStatus GetNpcMoveStatus(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId);

NpcMoveStatus GetNpcMoveStatusForEntity(
        const NpcNavigationRuntime& runtime,
        engine::Entity entity);

bool UpdateNpcFootstepCadence(
        NpcNavigationRecord& record,
        bool active,
        float resolvedHorizontalDistance);

bool UpdateNpcFootstepAnimationPhase(
        NpcNavigationRecord& record,
        bool active,
        uint32_t animationIndex,
        float normalizedPhase,
        float normalizedPhaseAdvance,
        const std::array<float, 2>& footstepPhases);

void UpdateNpcFootstepEventsSystem(
        engine::World& world,
        engine::AssetManager& assets,
        NpcNavigationRuntime& runtime,
        const NpcDefinitionCatalog& definitions,
        float dt);

// Returns a unit presentation-facing direction, or zero when path intent does
// not provide a stable direction and the caller should preserve current yaw.
Vector2 ResolveNpcLocomotionFacingDirection(
        Vector2 preferredVelocity,
        Vector2 actualVelocity);

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
        bool freezeAi = false,
        const Vector3* playerPosition = nullptr);

} // namespace game
