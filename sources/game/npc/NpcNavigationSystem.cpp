#include "game/npc/NpcNavigationSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/ai/NpcAiSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace game {
namespace {

constexpr float ArrivalTolerance = 0.10f;
constexpr float MovementDistanceEpsilon = 0.0001f;
constexpr float SteeringRecoveryTriggerSeconds = 0.75f;
constexpr float ReplanTriggerSeconds = 1.50f;
constexpr float ReplanCooldownSeconds = 0.50f;
constexpr float DriftCheckIntervalSeconds = 0.25f;
constexpr uint32_t MaximumReplans = 3;
constexpr float TurnRateRadiansPerSecond = 6.283185307179586f;
constexpr float ActualMotionFacingInfluence = 0.35f;
constexpr float VisualStepSmoothingRate = 16.0f;
constexpr float VisualOffsetEpsilon = 0.0001f;
constexpr float NpcWalkFallbackStepDistanceWorld = 0.75f;
constexpr float NpcRunFallbackStepDistanceWorld = 1.20f;
constexpr float PlayerAvoidancePredictionSeconds = 1.25f;
constexpr float PlayerAvoidancePadding = 0.15f;

size_t ActionIndex(NpcAction action)
{
    const size_t index = static_cast<size_t>(action);
    return index < kNpcActionCount ? index : 0;
}

void ClearNpcFootstepAnimationPhase(NpcNavigationRecord& record)
{
    record.footstepPreviousPhase = 0.0f;
    record.footstepAnimationIndex = engine::InvalidModelAnimationIndex;
    record.footstepPhaseValid = false;
}

void SetDiagnostic(NpcNavigationRecord& record, const char* message)
{
    std::snprintf(
            record.diagnostic.data(),
            record.diagnostic.size(),
            "%s",
            message != nullptr ? message : "");
}

void SetDiagnostic(NpcNavigationRecord& record, SectorNavigationQueryStatus status)
{
    SetDiagnostic(record, SectorNavigationQueryStatusName(status));
}

NpcNavigationRecord* FindRecord(
        NpcNavigationRuntime& runtime,
        std::string_view instanceId)
{
    const auto found = std::find_if(
            runtime.records.begin(),
            runtime.records.end(),
            [instanceId](const NpcNavigationRecord& record) {
                return record.occupied && record.instanceId == instanceId;
            });
    return found == runtime.records.end() ? nullptr : &*found;
}

const NpcNavigationRecord* FindRecord(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId)
{
    const auto found = std::find_if(
            runtime.records.begin(),
            runtime.records.end(),
            [instanceId](const NpcNavigationRecord& record) {
                return record.occupied && record.instanceId == instanceId;
            });
    return found == runtime.records.end() ? nullptr : &*found;
}

NpcNavigationRecord* FindRecord(
        NpcNavigationRuntime& runtime,
        engine::Entity entity)
{
    const auto found = std::find_if(
            runtime.records.begin(), runtime.records.end(),
            [entity](const NpcNavigationRecord& record) {
                return record.occupied && record.entity == entity;
            });
    return found == runtime.records.end() ? nullptr : &*found;
}

const NpcNavigationRecord* FindRecord(
        const NpcNavigationRuntime& runtime,
        engine::Entity entity)
{
    const auto found = std::find_if(
            runtime.records.begin(), runtime.records.end(),
            [entity](const NpcNavigationRecord& record) {
                return record.occupied && record.entity == entity;
            });
    return found == runtime.records.end() ? nullptr : &*found;
}

bool IsActive(NpcMovePhase phase)
{
    return phase == NpcMovePhase::FollowingPath;
}

void ResetProgressTracking(NpcNavigationRecord& record)
{
    ResetNpcWaypointProgressTracking(record);
}

void ReleasePath(
        SectorNavigationWorld& navigation,
        NpcNavigationRecord& record,
        engine::World* world = nullptr)
{
    if (world != nullptr && record.holdsDoor && record.doorId > 0) {
        world->ForEach<SectorDoor, SectorDoorOpenControl>(
                [&record](engine::Entity, SectorDoor& door,
                        SectorDoorOpenControl& control) {
                    if (door.placedObjectId == record.doorId
                            && control.navigationHolderCount > 0) {
                        --control.navigationHolderCount;
                    }
                });
    }
    if (!IsNull(record.pathHandle)) {
        navigation.ReleasePathRecord(record.pathHandle);
        record.pathHandle = {};
    }
    record.cornerCount = 0;
    record.nextCorner = 0;
    record.corridorTileCount = 0;
    record.pathTileRevision = 0;
    record.tileReplanPending = false;
    record.doorPhase = NpcDoorTraversalPhase::None;
    record.doorId = 0;
    record.doorDirection = SectorNavigationDoorDirection::None;
    record.doorLanding = {};
    record.doorWaitSeconds = 0.0f;
    record.holdsDoor = false;
    ResetProgressTracking(record);
}

void SetTerminal(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        NpcNavigationRecord& record,
        NpcMovePhase phase,
        SectorNavigationQueryStatus status,
        const char* diagnostic)
{
    ReleasePath(navigation, record, &world);
    record.phase = phase;
    record.lastQueryStatus = status;
    record.preferredVelocity = {};
    record.desiredVelocity = {};
    record.actualVelocity = {};
    record.crowdNeighborCount = 0;
    record.crowdNearestNeighborDistance = 0.0f;
    record.playerAvoidanceActive = false;
    ResetProgressTracking(record);
    record.replanCooldownSeconds = 0.0f;
    SetDiagnostic(record, diagnostic);
    if (phase == NpcMovePhase::Arrived) ++runtime.counters.arrivals;
    else if (phase == NpcMovePhase::Cancelled) ++runtime.counters.cancellations;
    else if (phase == NpcMovePhase::Failed) ++runtime.counters.failures;
}

bool CopySuccessfulPath(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRecord& record,
        const SectorNavigationPathResult& path)
{
    ReleasePath(navigation, record, &world);
    record.pathHandle = navigation.AllocatePathRecord();
    if (IsNull(record.pathHandle)) return false;
    record.cornerCount = std::min(path.cornerCount, record.corners.size());
    std::copy_n(path.corners.begin(), record.cornerCount, record.corners.begin());
    std::copy_n(path.cornerDoorIds.begin(), record.cornerCount,
            record.cornerDoorIds.begin());
    std::copy_n(path.cornerDoorDirections.begin(), record.cornerCount,
            record.cornerDoorDirections.begin());
    std::copy_n(path.cornerDoorLandings.begin(), record.cornerCount,
            record.cornerDoorLandings.begin());
    record.nextCorner = 0;
    record.corridorTileCount = std::min(
            path.corridorTileCount, record.corridorTiles.size());
    std::copy_n(path.corridorTiles.begin(), record.corridorTileCount,
            record.corridorTiles.begin());
    record.pathTileRevision = path.tileRevision;
    record.tileReplanPending = false;
    while (record.nextCorner < record.cornerCount) {
        if (record.cornerDoorIds[record.nextCorner] > 0) break;
        const Vector3& corner = record.corners[record.nextCorner];
        const float dx = corner.x - path.projectedStart.x;
        const float dz = corner.z - path.projectedStart.z;
        if (std::sqrt(dx * dx + dz * dz) > ArrivalTolerance) break;
        ++record.nextCorner;
    }
    record.projectedDestination = path.projectedDestination;
    record.lastQueryStatus = path.status;
    return true;
}

NpcMoveRequestResult FailRequest(SectorNavigationQueryStatus status, const char* message)
{
    return {false, status, 0,
            message != nullptr ? message : SectorNavigationQueryStatusName(status)};
}

uint64_t AllocateRequestId(NpcNavigationRuntime& runtime)
{
    uint64_t result = runtime.nextRequestId++;
    if (result == 0) result = runtime.nextRequestId++;
    return result;
}

float ShortestAngleDelta(float from, float to)
{
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float TwoPi = Pi * 2.0f;
    float delta = std::fmod(to - from, TwoPi);
    if (delta > Pi) delta -= TwoPi;
    if (delta < -Pi) delta += TwoPi;
    return delta;
}

void ResolveNpcAnimations(
        engine::World& world,
        engine::AssetManager& assets,
        const NpcDefinitionCatalog& definitions,
        engine::Entity entity)
{
    if (!world.IsAlive(entity)
            || !world.Has<NpcRuntimeInstance>(entity)
            || !world.Has<NpcAnimationState>(entity)
            || !world.Has<engine::AnimatedModelInstance>(entity)
            || !world.Has<engine::AnimatedModelAnimator>(entity)) {
        return;
    }
    NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(entity);
    NpcAnimationState& state = world.Get<NpcAnimationState>(entity);
    engine::AnimatedModelInstance& instance =
            world.Get<engine::AnimatedModelInstance>(entity);
    engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
    const NpcDefinition* definition = FindNpcDefinition(definitions, npc.definitionId);
    if (!state.resolved) {
        if (asset == nullptr || definition == nullptr) return;
        state.missingAnimationMask = 0;
        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            const size_t index = ActionIndex(metadata.action);
            const NpcActionDefinition& action = GetNpcAction(*definition, metadata.action);
            state.animationSpeeds[index] = action.animationSpeed;
            state.animationIndices[index] = action.animation.empty()
                    ? engine::InvalidModelAnimationIndex
                    : engine::FindModelAnimationIndex(
                            *asset, action.animation.c_str());
            if (state.animationIndices[index] == engine::InvalidModelAnimationIndex) {
                state.missingAnimationMask |= static_cast<uint8_t>(1u << index);
                if (!action.animation.empty()) {
                    std::fprintf(
                            stderr,
                            "[NPC WARNING] NPC '%s' semantic action '%s' has no model animation '%s'.\n",
                            npc.instanceId.c_str(),
                            metadata.displayName,
                            action.animation.c_str());
                }
            }
        }
        state.blendSeconds = definition->animationBlendSeconds;
        state.resolved = true;
        const uint32_t idleIndex = state.animationIndices[ActionIndex(NpcAction::Idle)];
        if (idleIndex != engine::InvalidModelAnimationIndex) {
            engine::SetAnimatedModelAnimation(animator, idleIndex, 0.0f, false);
            animator.speed = state.animationSpeeds[ActionIndex(NpcAction::Idle)];
        }
        state.appliedAction = NpcAction::Idle;
    }

    NpcCombatState* combat = world.Has<NpcCombatState>(entity)
            ? &world.Get<NpcCombatState>(entity)
            : nullptr;
    if (combat != nullptr && combat->dead) {
        const size_t deathIndex = ActionIndex(NpcAction::Death);
        if (combat->deathAnimationRequested) {
            combat->deathAnimationRequested = false;
            if (state.animationIndices[deathIndex]
                    == engine::InvalidModelAnimationIndex) {
                animator.playing = false;
                animator.targetAnimationIndex =
                        engine::InvalidModelAnimationIndex;
                animator.targetFrame = 0.0f;
                animator.targetFinished = false;
                animator.transitionDurationSeconds = 0.0f;
                animator.transitionElapsedSeconds = 0.0f;
                combat->deathAnimationComplete = true;
                return;
            }
            engine::SetAnimatedModelAnimation(
                    animator,
                    state.animationIndices[deathIndex],
                    state.blendSeconds,
                    true);
            animator.speed = state.animationSpeeds[deathIndex];
            engine::SetAnimatedModelAnimationLoop(
                    animator, state.animationIndices[deathIndex], false);
            state.appliedAction = NpcAction::Death;
        }
        if (state.appliedAction == NpcAction::Death
                && engine::IsAnimatedModelAnimationFinished(
                        animator, state.animationIndices[deathIndex])) {
            combat->deathAnimationComplete = true;
        }
        return;
    }

    if (combat != nullptr && combat->hurtAnimationRequested) {
        combat->hurtAnimationRequested = false;
        const size_t hurtIndex = ActionIndex(NpcAction::Hurt);
        if (state.animationIndices[hurtIndex]
                != engine::InvalidModelAnimationIndex) {
            engine::SetAnimatedModelAnimation(
                    animator,
                    state.animationIndices[hurtIndex],
                    state.blendSeconds,
                    true);
            animator.speed = state.animationSpeeds[hurtIndex];
            engine::SetAnimatedModelAnimationLoop(
                    animator, state.animationIndices[hurtIndex], false);
            state.appliedAction = NpcAction::Hurt;
            combat->hurtAnimationPlaying = true;
        } else {
            combat->hurtAnimationPlaying = false;
            return;
        }
    }
    if (combat != nullptr
            && combat->staggerRemainingSeconds > 0.0f
            && state.animationIndices[ActionIndex(NpcAction::Hurt)]
                    == engine::InvalidModelAnimationIndex) {
        return;
    }
    if (combat != nullptr && combat->hurtAnimationPlaying) {
        const uint32_t hurtIndex = state.animationIndices[
                ActionIndex(NpcAction::Hurt)];
        if (!engine::IsAnimatedModelAnimationFinished(
                    animator, hurtIndex)) return;
        combat->hurtAnimationPlaying = false;
    }
    ApplyNpcSemanticAnimation(state, animator, npc.action);
}

bool Replan(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        NpcNavigationRecord& record)
{
    if (!world.IsAlive(record.entity)
            || !world.Has<SectorObjectTransform>(record.entity)) {
        return false;
    }
    const SectorObjectTransform& transform =
            world.Get<SectorObjectTransform>(record.entity);
    const bool canOpenDoors = world.Has<NpcRuntimeInstance>(record.entity)
            && world.Get<NpcRuntimeInstance>(record.entity).canOpenDoors;
    const SectorNavigationPathResult path = navigation.FindPath(
            transform.position,
            record.projectedDestination,
            {canOpenDoors});
    record.lastQueryStatus = path.status;
    if (path.status != SectorNavigationQueryStatus::Success
            || !CopySuccessfulPath(world, navigation, record, path)) {
        return false;
    }
    ++record.replanCount;
    ++runtime.counters.replans;
    ResetProgressTracking(record);
    record.replanCooldownSeconds = ReplanCooldownSeconds;
    SetDiagnostic(record, "path replanned from actual position");
    return true;
}

SectorFpsVerticalContext BuildVerticalContext(
        const SectorCollisionWorld& collisionWorld,
        int sectorId)
{
    SectorFpsVerticalContext result;
    SectorCollisionHeights heights;
    if (sectorId != 0
            && collisionWorld.GetSectorFloorCeiling(sectorId, &heights)) {
        result.hasSector = true;
        result.floorZ = heights.floorZ;
        result.ceilingZ = heights.ceilingZ;
    }
    return result;
}

SectorCollisionMoveResult ResolveNpcHorizontalMovement(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const SectorTopologyMap& map,
        const NpcNavigationRuntime& runtime,
        const NpcNavigationRecord& record,
        const SectorDoorPlayerObstacle* playerObstacle,
        const SectorCollisionMoveConfig& moveConfig,
        const SectorCollisionMoveState& moveState,
        Vector2 desiredDelta)
{
    SectorCollisionMoveResult result = collisionWorld.ResolveMovement(
            moveState, desiredDelta, moveConfig);
    result = ResolveSectorDoorDynamicCollidersForPlayerMovement(
            moveState, result, moveConfig, doorColliders);
    const SectorFpsVerticalContext sectorContext = BuildVerticalContext(
            collisionWorld, result.currentSectorId);
    result = ResolveSectorStaticModelCollidersForPlayerMovement(
            moveState,
            result,
            moveConfig,
            sectorContext,
            staticColliders);
    if (map.previewSettings.npcToNpcCollisionEnabled) {
        result = ResolveNpcCollisionCylindersForMovement(
                moveState,
                result,
                moveConfig,
                record.placedObjectId,
                runtime.collisionCylinders.data(),
                runtime.collisionCylinders.size());
    }
    if (playerObstacle != nullptr) {
        const NpcCollisionCylinder playerCylinder{
                -1,
                playerObstacle->feetPosition,
                playerObstacle->radius,
                playerObstacle->height};
        result = ResolveNpcCollisionCylindersForMovement(
                moveState,
                result,
                moveConfig,
                record.placedObjectId,
                &playerCylinder,
                1);
    }
    return result;
}

Vector2 ApplyNpcMovementResult(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        NpcNavigationRuntime& runtime,
        const NpcNavigationRecord& record,
        const SectorCollisionMoveConfig& moveConfig,
        const SectorCollisionMoveResult& result,
        SectorObjectTransform& transform,
        SectorObject& object,
        SectorObjectVisualOffset* visualOffset,
        bool& capturedStepOffset)
{
    const Vector2 previousXZ{transform.position.x, transform.position.z};
    const float previousPhysicalY = transform.position.y;
    const float previousVisualY = previousPhysicalY
            + (visualOffset != nullptr ? visualOffset->position.y : 0.0f);
    transform.position.x = result.positionXZ.x;
    transform.position.z = result.positionXZ.y;
    object.currentSectorId = result.currentSectorId;
    SectorFpsVerticalContext support = BuildVerticalContext(
            collisionWorld, object.currentSectorId);
    if (support.hasSector) {
        SectorFpsControllerState supportState;
        supportState.feetPosition = transform.position;
        supportState.feetPosition.y = previousPhysicalY;
        supportState.currentSectorId = object.currentSectorId;
        supportState.grounded = true;
        SectorFpsControllerConfig supportConfig;
        supportConfig.playerRadius = moveConfig.radius;
        supportConfig.playerHeight = moveConfig.playerHeight;
        supportConfig.stepHeight = moveConfig.stepHeight;
        support = BuildSectorStaticModelVerticalContext(
                support,
                supportState,
                supportConfig,
                staticColliders);
        const float floorDelta = support.floorZ - previousPhysicalY;
        if (std::fabs(floorDelta) <= moveConfig.stepHeight + 0.001f) {
            transform.position.y = support.floorZ;
            if (visualOffset != nullptr
                    && std::fabs(transform.position.y - previousPhysicalY)
                            > 0.001f) {
                visualOffset->position.y = previousVisualY - transform.position.y;
                capturedStepOffset = true;
            }
        }
    }
    for (NpcCollisionCylinder& cylinder : runtime.collisionCylinders) {
        if (cylinder.stableId == record.placedObjectId) {
            cylinder.feetPosition = transform.position;
            break;
        }
    }
    return {
            transform.position.x - previousXZ.x,
            transform.position.z - previousXZ.y};
}

Vector2 ApplyFriendlyPlayerAvoidance(
        const NpcNavigationRecord& record,
        Vector3 position,
        Vector2 preferredVelocity,
        float npcRadius,
        float npcHeight,
        float queryRange,
        const SectorDoorPlayerObstacle* playerObstacle,
        bool& outActive)
{
    outActive = false;
    if (playerObstacle == nullptr
            || Vector2LengthSqr(preferredVelocity) <= MovementDistanceEpsilon
            || position.y + npcHeight <= playerObstacle->feetPosition.y
            || position.y >= playerObstacle->feetPosition.y + playerObstacle->height) {
        return preferredVelocity;
    }
    const Vector2 toPlayer{
            playerObstacle->feetPosition.x - position.x,
            playerObstacle->feetPosition.z - position.z};
    const float distanceSquared = Vector2LengthSqr(toPlayer);
    if (distanceSquared > queryRange * queryRange) return preferredVelocity;
    const float speedSquared = Vector2LengthSqr(preferredVelocity);
    const float time = std::clamp(
            Vector2DotProduct(toPlayer, preferredVelocity) / speedSquared,
            0.0f,
            PlayerAvoidancePredictionSeconds);
    const Vector2 closest = Vector2Subtract(
            toPlayer, Vector2Scale(preferredVelocity, time));
    const float clearance = npcRadius + playerObstacle->radius
            + PlayerAvoidancePadding;
    const float closestDistance = Vector2Length(closest);
    if (time <= 0.0f || closestDistance >= clearance) return preferredVelocity;

    Vector2 away = Vector2Negate(toPlayer);
    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
    if (distance > MovementDistanceEpsilon) {
        away = Vector2Scale(away, 1.0f / distance);
    } else {
        away = (record.placedObjectId & 1) != 0
                ? Vector2{1.0f, 0.0f} : Vector2{-1.0f, 0.0f};
    }
    Vector2 tangent{-away.y, away.x};
    const float cross = preferredVelocity.x * toPlayer.y
            - preferredVelocity.y * toPlayer.x;
    if (cross < -MovementDistanceEpsilon
            || (std::fabs(cross) <= MovementDistanceEpsilon
                && (record.placedObjectId & 1) == 0)) {
        tangent = Vector2Negate(tangent);
    }
    const float speed = std::sqrt(speedSquared);
    const float strength = std::clamp(
            1.0f - closestDistance / clearance, 0.0f, 1.0f);
    Vector2 adjusted = Vector2Add(
            preferredVelocity,
            Vector2Scale(Vector2Add(away, tangent), speed * strength));
    const float adjustedLength = Vector2Length(adjusted);
    if (adjustedLength > speed && adjustedLength > MovementDistanceEpsilon) {
        adjusted = Vector2Scale(adjusted, speed / adjustedLength);
    }
    outActive = true;
    return adjusted;
}

} // namespace

Vector2 ResolveNpcLocomotionFacingDirection(
        Vector2 preferredVelocity,
        Vector2 actualVelocity)
{
    const float preferredSpeed = Vector2Length(preferredVelocity);
    if (!std::isfinite(preferredSpeed)
            || preferredSpeed <= MovementDistanceEpsilon) {
        return {};
    }
    const Vector2 intent = Vector2Scale(
            preferredVelocity, 1.0f / preferredSpeed);
    const float actualSpeed = Vector2Length(actualVelocity);
    if (!std::isfinite(actualSpeed)
            || actualSpeed <= MovementDistanceEpsilon) {
        return intent;
    }
    const Vector2 motion = Vector2Scale(actualVelocity, 1.0f / actualSpeed);
    const float forwardAlignment = std::clamp(
            Vector2DotProduct(intent, motion), 0.0f, 1.0f);
    const float speedRatio = std::clamp(
            actualSpeed / preferredSpeed, 0.0f, 1.0f);
    const float influence = ActualMotionFacingInfluence
            * speedRatio * forwardAlignment;
    const Vector2 biased = Vector2Add(
            intent, Vector2Scale(motion, influence));
    const float biasedLength = Vector2Length(biased);
    return biasedLength > MovementDistanceEpsilon
            ? Vector2Scale(biased, 1.0f / biasedLength)
            : intent;
}

void ResetNpcWaypointProgressTracking(NpcNavigationRecord& record)
{
    record.stallSeconds = 0.0f;
    record.bestCornerDistance = 0.0f;
    record.trackedCornerIndex = SIZE_MAX;
    record.steeringRecoveryActive = false;
}

void UpdateNpcWaypointProgressTracking(
        NpcNavigationRecord& record,
        size_t cornerIndex,
        float cornerDistance,
        float agentRadius,
        float maximumSpeed,
        float dt)
{
    if (record.trackedCornerIndex != cornerIndex) {
        record.trackedCornerIndex = cornerIndex;
        record.bestCornerDistance = cornerDistance;
        record.stallSeconds = 0.0f;
        record.steeringRecoveryActive = false;
        return;
    }
    const float meaningfulProgress = std::max(
            0.0025f,
            std::min(agentRadius * 0.1f, maximumSpeed * 0.25f));
    if (record.bestCornerDistance - cornerDistance >= meaningfulProgress) {
        record.bestCornerDistance = cornerDistance;
        record.stallSeconds = 0.0f;
    } else {
        record.stallSeconds += std::max(0.0f, dt);
    }
    if (record.stallSeconds >= SteeringRecoveryTriggerSeconds) {
        record.steeringRecoveryActive = true;
    }
}

NpcAnimationApplyResult ApplyNpcSemanticAnimation(
        NpcAnimationState& state,
        engine::AnimatedModelAnimator& animator,
        NpcAction requested)
{
    if (!state.resolved) return NpcAnimationApplyResult::Unchanged;
    if (engine::IsAnimatedModelTransitioning(animator)) {
        if (requested != state.appliedAction) {
            state.pendingAction = requested;
            state.hasPendingAction = true;
            return NpcAnimationApplyResult::Queued;
        }
        state.hasPendingAction = false;
        return NpcAnimationApplyResult::Unchanged;
    }
    if (state.hasPendingAction) {
        requested = state.pendingAction;
        state.hasPendingAction = false;
    }
    if (requested == state.appliedAction) {
        return NpcAnimationApplyResult::Unchanged;
    }
    const size_t requestedIndex = ActionIndex(requested);
    const uint32_t animationIndex = state.animationIndices[requestedIndex];
    if (animationIndex == engine::InvalidModelAnimationIndex) {
        return NpcAnimationApplyResult::Missing;
    }
    engine::SetAnimatedModelAnimation(
            animator,
            animationIndex,
            state.blendSeconds,
            false);
    animator.speed = state.animationSpeeds[requestedIndex];
    const bool loop = requested != NpcAction::Attack
            && requested != NpcAction::Hurt
            && requested != NpcAction::Death;
    engine::SetAnimatedModelAnimationLoop(
            animator, animationIndex, loop);
    state.appliedAction = requested;
    return NpcAnimationApplyResult::Applied;
}

void UpdateNpcAnimationStateSystem(
        engine::World& world,
        engine::AssetManager& assets,
        const NpcDefinitionCatalog& definitions)
{
    world.ForEach<NpcRuntimeInstance, NpcAnimationState,
            engine::AnimatedModelInstance, engine::AnimatedModelAnimator>(
            [&world, &assets, &definitions](
                    engine::Entity entity,
                    NpcRuntimeInstance&,
                    NpcAnimationState&,
                    engine::AnimatedModelInstance&,
                    engine::AnimatedModelAnimator&) {
                ResolveNpcAnimations(world, assets, definitions, entity);
            });
}

void InitializeNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime)
{
    ShutdownNpcNavigationRuntime(world, navigation, runtime);
    runtime.records.reserve(navigation.Capacities().agentCapacity);
    runtime.collisionCylinders.reserve(navigation.Capacities().agentCapacity);
    world.ForEach<NpcRuntimeInstance, SectorDynamicModel>(
            [&runtime, &navigation](
                    engine::Entity entity,
                    NpcRuntimeInstance& npc,
                    SectorDynamicModel& model) {
                if (runtime.records.size() == runtime.records.capacity()
                        && !runtime.growthWarned) {
                    runtime.growthWarned = true;
                    ++runtime.counters.capacityWarnings;
                    std::fprintf(stderr,
                            "[NPC Navigation WARNING] Agent capacity exceeded; runtime allocation may occur.\n");
                }
                NpcNavigationRecord record;
                record.instanceId = npc.instanceId;
                record.placedObjectId = model.placedObjectId;
                record.entity = entity;
                record.agentHandle = navigation.AllocateAgentRecord();
                record.phase = NpcMovePhase::Idle;
                record.occupied = true;
                runtime.records.push_back(std::move(record));
            });
    if (runtime.collisionCylinders.capacity() < runtime.records.size()) {
        runtime.collisionCylinders.reserve(runtime.records.size());
    }
}

void ShutdownNpcNavigationRuntime(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime)
{
    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied) continue;
        if (world.IsAlive(record.entity)) {
            if (world.Has<NpcRuntimeInstance>(record.entity)) {
                world.Get<NpcRuntimeInstance>(record.entity).action = NpcAction::Idle;
            }
            if (world.Has<SectorObjectVisualOffset>(record.entity)) {
                world.Get<SectorObjectVisualOffset>(record.entity).position = {};
            }
        }
        ReleasePath(navigation, record, &world);
        if (!IsNull(record.agentHandle)) {
            navigation.ReleaseAgentRecord(record.agentHandle);
        }
    }
    const size_t capacity = runtime.records.capacity();
    runtime.records.clear();
    if (capacity > 0) runtime.records.reserve(capacity);
    const size_t collisionCapacity = runtime.collisionCylinders.capacity();
    runtime.collisionCylinders.clear();
    if (collisionCapacity > 0) {
        runtime.collisionCylinders.reserve(collisionCapacity);
    }
    runtime.counters = {};
    runtime.growthWarned = false;
}

bool DeactivateNpcNavigation(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        engine::Entity entity)
{
    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || record.entity != entity) continue;
        ReleasePath(navigation, record, &world);
        if (!IsNull(record.agentHandle)) {
            navigation.ReleaseAgentRecord(record.agentHandle);
            record.agentHandle = {};
        }
        record.preferredVelocity = {};
        record.desiredVelocity = {};
        record.actualVelocity = {};
        record.occupied = false;
        runtime.collisionCylinders.erase(
                std::remove_if(
                        runtime.collisionCylinders.begin(),
                        runtime.collisionCylinders.end(),
                        [&record](const NpcCollisionCylinder& cylinder) {
                            return cylinder.stableId == record.placedObjectId;
                        }),
                runtime.collisionCylinders.end());
        return true;
    }
    return false;
}

static NpcMoveRequestResult RequestNpcMoveRecord(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        NpcNavigationRecord* record,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority,
        float movementSpeedOverride)
{
    if (record == nullptr || !world.IsAlive(record->entity)
            || !world.Has<SectorObjectTransform>(record->entity)
            || !world.Has<SectorObject>(record->entity)) {
        return FailRequest(SectorNavigationQueryStatus::InvalidAgent, "NPC instance was not found");
    }
    if (authority == NpcMoveAuthority::Script
            && world.Has<NpcAiState>(record->entity)
            && world.Get<NpcAiState>(record->entity).awareness
                    != NpcAwarenessState::Unaware) {
        return FailRequest(
                SectorNavigationQueryStatus::InvalidAgent,
                "player detected; AI took control");
    }
    const bool replacingPatrol = IsActive(record->phase)
            && authority == NpcMoveAuthority::Script
            && record->authority == NpcMoveAuthority::Patrol;
    if (IsActive(record->phase) && !replacingPatrol) {
        if (record->authority == NpcMoveAuthority::Script) {
            return FailRequest(
                    SectorNavigationQueryStatus::InvalidAgent,
                    authority == NpcMoveAuthority::Script
                            ? "NPC already has an active scripted move"
                            : "NPC movement is controlled by a script");
        }
        return FailRequest(
                SectorNavigationQueryStatus::InvalidAgent,
                record->authority == NpcMoveAuthority::Ai
                        ? "NPC movement is controlled by AI"
                        : "NPC already has an active move");
    }
    if (navigation.State() != SectorNavigationState::Ready) {
        return FailRequest(SectorNavigationQueryStatus::NavigationUnavailable, "navigation is not ready");
    }
    if (!navigation.IsAgentRecordValid(record->agentHandle)) {
        record->agentHandle = navigation.AllocateAgentRecord();
        if (IsNull(record->agentHandle)) {
            ++runtime.counters.capacityWarnings;
            return FailRequest(
                    SectorNavigationQueryStatus::CapacityExceeded,
                    "navigation agent capacity was exceeded");
        }
    }
    const SectorObjectTransform& transform =
            world.Get<SectorObjectTransform>(record->entity);
    const SectorObject& object = world.Get<SectorObject>(record->entity);
    const int destinationSector = collisionWorld.FindSectorContainingPointPreferCurrent(
            destinationXZ,
            object.currentSectorId);
    SectorCollisionHeights destinationHeights;
    if (destinationSector == 0
            || !collisionWorld.GetSectorFloorCeiling(
                    destinationSector, &destinationHeights)) {
        return FailRequest(
                SectorNavigationQueryStatus::DestinationNotOnNavmesh,
                "destination is outside sector collision");
    }
    const Vector3 destination{
            destinationXZ.x,
            destinationHeights.floorZ,
            destinationXZ.y};
    const SectorNavigationPathResult path = navigation.FindPath(
            transform.position,
            destination,
            {world.Has<NpcRuntimeInstance>(record->entity)
                    && world.Get<NpcRuntimeInstance>(record->entity).canOpenDoors});
    if (path.status != SectorNavigationQueryStatus::Success) {
        return FailRequest(path.status, SectorNavigationQueryStatusName(path.status));
    }
    if (!CopySuccessfulPath(world, navigation, *record, path)) {
        ++runtime.counters.capacityWarnings;
        return FailRequest(
                SectorNavigationQueryStatus::CapacityExceeded,
                "navigation path record capacity was exceeded");
    }
    record->phase = NpcMovePhase::FollowingPath;
    record->gait = gait;
    record->movementSpeedOverride = std::isfinite(movementSpeedOverride)
            && movementSpeedOverride > 0.0f
            ? movementSpeedOverride : 0.0f;
    record->authority = authority;
    record->requestId = AllocateRequestId(runtime);
    record->requestedDestinationXZ = destinationXZ;
    record->projectedDestination = path.projectedDestination;
    record->desiredVelocity = {};
    record->actualVelocity = {};
    record->footstepDistanceWorld = 0.0f;
    ClearNpcFootstepAnimationPhase(*record);
    record->footstepMovementActive = false;
    record->footstepEvent = false;
    ResetProgressTracking(*record);
    record->replanCooldownSeconds = 0.0f;
    record->driftCheckSeconds = 0.0f;
    record->replanCount = 0;
    SetDiagnostic(*record, "following complete path");
    ++runtime.counters.requests;
    return {true, SectorNavigationQueryStatus::Success, record->requestId, {}};
}

NpcMoveRequestResult RequestNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority,
        float movementSpeedOverride)
{
    if (!IsValidNpcInstanceId(instanceId)) {
        return FailRequest(SectorNavigationQueryStatus::InvalidAgent, "invalid NPC instance ID");
    }
    return RequestNpcMoveRecord(
            world, navigation, collisionWorld, runtime,
            FindRecord(runtime, instanceId), destinationXZ, gait, authority,
            movementSpeedOverride);
}

NpcMoveRequestResult RequestNpcMoveForEntity(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        engine::Entity entity,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority,
        float movementSpeedOverride)
{
    return RequestNpcMoveRecord(
            world, navigation, collisionWorld, runtime,
            FindRecord(runtime, entity), destinationXZ, gait, authority,
            movementSpeedOverride);
}

NpcMoveRequestResult RetargetNpcAiMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait)
{
    if (!IsValidNpcInstanceId(instanceId)) {
        return FailRequest(
                SectorNavigationQueryStatus::InvalidAgent,
                "invalid NPC instance ID");
    }
    NpcNavigationRecord* record = FindRecord(runtime, instanceId);
    if (record == nullptr || !world.IsAlive(record->entity)
            || !world.Has<SectorObjectTransform>(record->entity)
            || !world.Has<SectorObject>(record->entity)) {
        return FailRequest(
                SectorNavigationQueryStatus::InvalidAgent,
                "NPC instance was not found");
    }
    const bool replacing = IsActive(record->phase);
    if (replacing && record->authority != NpcMoveAuthority::Ai
            && record->authority != NpcMoveAuthority::Patrol) {
        return FailRequest(
                SectorNavigationQueryStatus::InvalidAgent,
                "NPC movement is not controlled by AI");
    }
    if (navigation.State() != SectorNavigationState::Ready) {
        return FailRequest(
                SectorNavigationQueryStatus::NavigationUnavailable,
                "navigation is not ready");
    }
    if (!navigation.IsAgentRecordValid(record->agentHandle)) {
        if (replacing) {
            return FailRequest(
                    SectorNavigationQueryStatus::InvalidAgent,
                    "active AI navigation agent was invalid");
        }
        record->agentHandle = navigation.AllocateAgentRecord();
        if (IsNull(record->agentHandle)) {
            ++runtime.counters.capacityWarnings;
            return FailRequest(
                    SectorNavigationQueryStatus::CapacityExceeded,
                    "navigation agent capacity was exceeded");
        }
    }

    const SectorObjectTransform& transform =
            world.Get<SectorObjectTransform>(record->entity);
    const SectorObject& object = world.Get<SectorObject>(record->entity);
    const int destinationSector =
            collisionWorld.FindSectorContainingPointPreferCurrent(
                    destinationXZ,
                    object.currentSectorId);
    SectorCollisionHeights destinationHeights;
    if (destinationSector == 0
            || !collisionWorld.GetSectorFloorCeiling(
                    destinationSector,
                    &destinationHeights)) {
        return FailRequest(
                SectorNavigationQueryStatus::DestinationNotOnNavmesh,
                "destination is outside sector collision");
    }
    const Vector3 destination{
            destinationXZ.x,
            destinationHeights.floorZ,
            destinationXZ.y};
    const bool canOpenDoors = world.Has<NpcRuntimeInstance>(record->entity)
            && world.Get<NpcRuntimeInstance>(record->entity).canOpenDoors;
    const SectorNavigationPathResult path = navigation.FindPath(
            transform.position,
            destination,
            {canOpenDoors});
    if (path.status != SectorNavigationQueryStatus::Success) {
        return FailRequest(
                path.status,
                SectorNavigationQueryStatusName(path.status));
    }

    // The old path remains live until this point. Releasing its fixed-capacity
    // record guarantees CopySuccessfulPath can reuse that capacity.
    if (!CopySuccessfulPath(world, navigation, *record, path)) {
        ++runtime.counters.capacityWarnings;
        return FailRequest(
                SectorNavigationQueryStatus::CapacityExceeded,
                "navigation path record capacity was exceeded");
    }
    record->phase = NpcMovePhase::FollowingPath;
    record->gait = gait;
    record->authority = NpcMoveAuthority::Ai;
    record->requestId = AllocateRequestId(runtime);
    record->requestedDestinationXZ = destinationXZ;
    record->projectedDestination = path.projectedDestination;
    if (!replacing) {
        record->desiredVelocity = {};
        record->actualVelocity = {};
        record->footstepDistanceWorld = 0.0f;
        ClearNpcFootstepAnimationPhase(*record);
        record->footstepMovementActive = false;
    }
    record->footstepEvent = false;
    ResetProgressTracking(*record);
    record->replanCooldownSeconds = 0.0f;
    record->driftCheckSeconds = 0.0f;
    record->replanCount = 0;
    SetDiagnostic(*record, replacing
            ? "following atomically retargeted AI path"
            : "following AI pursuit path");
    ++runtime.counters.requests;
    return {true, SectorNavigationQueryStatus::Success, record->requestId, {}};
}

bool CancelNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        uint64_t expectedRequestId)
{
    NpcNavigationRecord* record = FindRecord(runtime, instanceId);
    if (record == nullptr || !IsActive(record->phase)
            || (expectedRequestId != 0
                && record->requestId != expectedRequestId)) {
        return false;
    }
    if (world.IsAlive(record->entity)
            && world.Has<NpcRuntimeInstance>(record->entity)) {
        world.Get<NpcRuntimeInstance>(record->entity).action = NpcAction::Idle;
    }
    SetTerminal(
            world,
            navigation,
            runtime,
            *record,
            NpcMovePhase::Cancelled,
            SectorNavigationQueryStatus::Cancelled,
            "movement cancelled");
    return true;
}

bool CancelNpcMoveForEntity(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        engine::Entity entity,
        uint64_t expectedRequestId)
{
    NpcNavigationRecord* record = FindRecord(runtime, entity);
    if (record == nullptr || !IsActive(record->phase)
            || (expectedRequestId != 0 && record->requestId != expectedRequestId)) {
        return false;
    }
    if (world.IsAlive(record->entity)
            && world.Has<NpcRuntimeInstance>(record->entity)) {
        world.Get<NpcRuntimeInstance>(record->entity).action = NpcAction::Idle;
    }
    SetTerminal(
            world, navigation, runtime, *record,
            NpcMovePhase::Cancelled,
            SectorNavigationQueryStatus::Cancelled,
            "movement cancelled");
    return true;
}

static NpcMoveStatus MakeNpcMoveStatus(const NpcNavigationRecord* record)
{
    NpcMoveStatus status;
    if (record == nullptr) return status;
    status.found = true;
    status.phase = record->phase;
    status.gait = record->gait;
    status.authority = record->authority;
    status.requestId = record->requestId;
    status.queryStatus = record->lastQueryStatus;
    status.requestedDestinationXZ = record->requestedDestinationXZ;
    status.projectedDestination = record->projectedDestination;
    status.remainingCorners = record->cornerCount > record->nextCorner
            ? record->cornerCount - record->nextCorner : 0;
    status.desiredVelocity = record->desiredVelocity;
    status.actualVelocity = record->actualVelocity;
    status.stallSeconds = record->stallSeconds;
    status.replanCount = record->replanCount;
    status.message = record->diagnostic;
    return status;
}

NpcMoveStatus GetNpcMoveStatus(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId)
{
    return MakeNpcMoveStatus(FindRecord(runtime, instanceId));
}

NpcMoveStatus GetNpcMoveStatusForEntity(
        const NpcNavigationRuntime& runtime,
        engine::Entity entity)
{
    return MakeNpcMoveStatus(FindRecord(runtime, entity));
}

bool UpdateNpcFootstepCadence(
        NpcNavigationRecord& record,
        bool active,
        float resolvedHorizontalDistance)
{
    record.footstepMovementActive = active
            && std::isfinite(resolvedHorizontalDistance)
            && resolvedHorizontalDistance > MovementDistanceEpsilon;
    if (!active) {
        record.footstepDistanceWorld = 0.0f;
        record.footstepEvent = false;
        ClearNpcFootstepAnimationPhase(record);
        return false;
    }
    if (!std::isfinite(record.footstepDistanceWorld)
            || record.footstepDistanceWorld < 0.0f) {
        record.footstepDistanceWorld = 0.0f;
    }
    if (!record.footstepMovementActive) {
        record.footstepEvent = false;
        return false;
    }
    const float stepDistance = record.gait == NpcMoveGait::Run
            ? NpcRunFallbackStepDistanceWorld
            : NpcWalkFallbackStepDistanceWorld;
    record.footstepDistanceWorld += resolvedHorizontalDistance;
    if (record.footstepDistanceWorld < stepDistance) {
        record.footstepEvent = false;
        return false;
    }
    record.footstepDistanceWorld = std::fmod(
            record.footstepDistanceWorld,
            stepDistance);
    record.footstepEvent = true;
    return record.footstepEvent;
}

bool UpdateNpcFootstepAnimationPhase(
        NpcNavigationRecord& record,
        bool active,
        uint32_t animationIndex,
        float normalizedPhase,
        float normalizedPhaseAdvance,
        const std::array<float, 2>& footstepPhases)
{
    if (!active
            || animationIndex == engine::InvalidModelAnimationIndex
            || !std::isfinite(normalizedPhase)
            || normalizedPhase < 0.0f
            || normalizedPhase >= 1.0f) {
        ClearNpcFootstepAnimationPhase(record);
        return false;
    }
    if (!record.footstepPhaseValid
            || record.footstepAnimationIndex != animationIndex) {
        record.footstepPreviousPhase = normalizedPhase;
        record.footstepAnimationIndex = animationIndex;
        record.footstepPhaseValid = true;
        return false;
    }

    const float previousPhase = record.footstepPreviousPhase;
    record.footstepPreviousPhase = normalizedPhase;
    const bool skippedCompleteCycle =
            std::isfinite(normalizedPhaseAdvance)
            && normalizedPhaseAdvance >= 1.0f;
    if (skippedCompleteCycle) return true;

    for (const float phase : footstepPhases) {
        const bool crossed = normalizedPhase >= previousPhase
                ? phase > previousPhase && phase <= normalizedPhase
                : phase > previousPhase || phase <= normalizedPhase;
        if (crossed) return true;
    }
    return false;
}

void UpdateNpcFootstepEventsSystem(
        engine::World& world,
        engine::AssetManager& assets,
        NpcNavigationRuntime& runtime,
        const NpcDefinitionCatalog& definitions,
        float rawDt)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || !world.IsAlive(record.entity)
                || !world.Has<NpcRuntimeInstance>(record.entity)
                || !world.Has<NpcAnimationState>(record.entity)
                || !world.Has<engine::AnimatedModelInstance>(record.entity)
                || !world.Has<engine::AnimatedModelAnimator>(record.entity)) {
            ClearNpcFootstepAnimationPhase(record);
            continue;
        }

        const NpcRuntimeInstance& npc =
                world.Get<NpcRuntimeInstance>(record.entity);
        const NpcAnimationState& animationState =
                world.Get<NpcAnimationState>(record.entity);
        const NpcAction action = npc.action;
        if (!record.footstepMovementActive
                || (action != NpcAction::Walk && action != NpcAction::Run)) {
            record.footstepEvent = false;
            ClearNpcFootstepAnimationPhase(record);
            continue;
        }

        const NpcDefinition* definition =
                FindNpcDefinition(definitions, npc.definitionId);
        const size_t actionIndex = ActionIndex(action);
        const uint32_t animationIndex =
                animationState.animationIndices[actionIndex];
        const engine::AnimatedModelInstance& instance =
                world.Get<engine::AnimatedModelInstance>(record.entity);
        const engine::AnimatedModelAnimator& animator =
                world.Get<engine::AnimatedModelAnimator>(record.entity);
        const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
        if (definition == nullptr || asset == nullptr
                || !instance.poseReady || instance.poseFailed
                || animationIndex == engine::InvalidModelAnimationIndex) {
            ClearNpcFootstepAnimationPhase(record);
            continue;
        }

        const int keyframeCount = engine::ModelAnimationClipKeyframeCount(
                *asset,
                animationIndex);
        if (keyframeCount <= 0) {
            ClearNpcFootstepAnimationPhase(record);
            continue;
        }

        float frame = 0.0f;
        if (animator.targetAnimationIndex == animationIndex) {
            frame = animator.targetFrame;
        } else if (animator.animationIndex == animationIndex) {
            frame = animator.frame;
        } else {
            ClearNpcFootstepAnimationPhase(record);
            continue;
        }
        const float frameCount = static_cast<float>(keyframeCount);
        const float normalizedPhase = frame / frameCount;
        const float normalizedAdvance = animator.paused
                ? 0.0f
                : dt * engine::GltfAnimationFramesPerSecond
                        * animator.speed / frameCount;
        record.footstepDistanceWorld = 0.0f;
        record.footstepEvent = UpdateNpcFootstepAnimationPhase(
                record,
                true,
                animationIndex,
                normalizedPhase,
                normalizedAdvance,
                GetNpcAction(*definition, action).footstepPhases);
    }
}

void PrepareNpcDoorTraversalAndHoldsSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        float rawDt,
        bool freezeAi,
        int externalDoorHoldId)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    world.ForEach<SectorDoorOpenControl>(
            [](engine::Entity, SectorDoorOpenControl& control) {
                control.navigationHolderCount = 0;
            });

    const SectorNavigationSettings& settings = navigation.Settings();
    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || !IsActive(record.phase)
                || !world.IsAlive(record.entity)
                || !world.Has<NpcRuntimeInstance>(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)) {
            record.holdsDoor = false;
            continue;
        }
        const NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(record.entity);
        if (freezeAi && (record.authority == NpcMoveAuthority::Ai
                || record.authority == NpcMoveAuthority::Patrol)) continue;
        const SectorObjectTransform& transform =
                world.Get<SectorObjectTransform>(record.entity);
        if (record.doorPhase == NpcDoorTraversalPhase::None
                && record.nextCorner < record.cornerCount
                && record.cornerDoorIds[record.nextCorner] > 0) {
            record.doorPhase = NpcDoorTraversalPhase::Approaching;
            record.doorId = record.cornerDoorIds[record.nextCorner];
            record.doorDirection = record.cornerDoorDirections[record.nextCorner];
            record.doorLanding = record.cornerDoorLandings[record.nextCorner];
            record.doorWaitSeconds = 0.0f;
        }
        if (record.doorPhase == NpcDoorTraversalPhase::None) continue;

        const Vector3 stage = record.corners[record.nextCorner];
        const float dx = stage.x - transform.position.x;
        const float dz = stage.z - transform.position.z;
        if (record.doorPhase == NpcDoorTraversalPhase::Approaching
                && std::sqrt(dx * dx + dz * dz) <= ArrivalTolerance) {
            record.doorPhase = NpcDoorTraversalPhase::WaitingForClearance;
            record.holdsDoor = npc.canOpenDoors;
            SetDiagnostic(record, npc.canOpenDoors
                    ? "holding door open; waiting for physical clearance"
                    : "waiting for an already-open door");
        }
        if (record.doorPhase == NpcDoorTraversalPhase::WaitingForClearance) {
            record.doorWaitSeconds += dt;
            record.holdsDoor = npc.canOpenDoors;
            SectorNavigationDoorLinkState linkState =
                    SectorNavigationDoorLinkState::Disabled;
            const bool linkExists = navigation.GetDoorLinkRuntimeState(
                    record.doorId, linkState);
            if (!linkExists || linkState == SectorNavigationDoorLinkState::Disabled) {
                if (!Replan(world, navigation, runtime, record)) {
                    SetTerminal(world, navigation, runtime, record,
                            NpcMovePhase::Failed,
                            SectorNavigationQueryStatus::NoPath,
                            "door link became unavailable");
                }
                continue;
            }
            const bool clear = SectorDoorTraversalIsClear(
                    record.doorId,
                    stage,
                    record.doorLanding,
                    settings.agentRadius,
                    settings.agentHeight,
                    doorColliders);
            if (clear && (npc.canOpenDoors
                            || linkState == SectorNavigationDoorLinkState::Clear)) {
                const NpcNavigationRecord* crossing = nullptr;
                const NpcNavigationRecord* bestWaiting = nullptr;
                float bestDistanceSquared = 0.0f;
                for (const NpcNavigationRecord& candidate : runtime.records) {
                    if (!candidate.occupied || !IsActive(candidate.phase)
                            || candidate.doorId != record.doorId
                            || !world.IsAlive(candidate.entity)
                            || !world.Has<SectorObjectTransform>(candidate.entity)) {
                        continue;
                    }
                    if (candidate.doorPhase == NpcDoorTraversalPhase::Crossing) {
                        crossing = &candidate;
                        break;
                    }
                    if (candidate.doorPhase
                            != NpcDoorTraversalPhase::WaitingForClearance
                            || candidate.nextCorner >= candidate.cornerCount) {
                        continue;
                    }
                    const Vector3 candidatePosition =
                            world.Get<SectorObjectTransform>(candidate.entity)
                                    .position;
                    const Vector3 candidateStage =
                            candidate.corners[candidate.nextCorner];
                    const float candidateDistanceSquared =
                            Vector3DistanceSqr(candidatePosition, candidateStage);
                    if (bestWaiting == nullptr
                            || candidateDistanceSquared
                                    < bestDistanceSquared - 0.0001f
                            || (std::fabs(candidateDistanceSquared
                                            - bestDistanceSquared) <= 0.0001f
                                && candidate.placedObjectId
                                        < bestWaiting->placedObjectId)) {
                        bestWaiting = &candidate;
                        bestDistanceSquared = candidateDistanceSquared;
                    }
                }
                if (crossing == nullptr && bestWaiting == &record) {
                    record.doorPhase = NpcDoorTraversalPhase::Crossing;
                    ResetProgressTracking(record);
                    SetDiagnostic(record, "crossing physically clear door");
                } else {
                    SetDiagnostic(record, "queued for exclusive door crossing");
                }
            } else if (!npc.canOpenDoors && record.doorWaitSeconds >= 0.5f) {
                if (!Replan(world, navigation, runtime, record)) {
                    SetTerminal(world, navigation, runtime, record,
                            NpcMovePhase::Failed,
                            SectorNavigationQueryStatus::NoPath,
                            "NPC cannot open the required door");
                }
                continue;
            }
        }

    }

    // Derive holder counts only after every record has updated its traversal
    // state, so replans/cancellations earlier in the pass cannot undercount a
    // different NPC that uses the same door.
    for (NpcNavigationRecord& record : runtime.records) {
        if (record.occupied && IsActive(record.phase)
                && record.holdsDoor && record.doorId > 0) {
            world.ForEach<SectorDoor, SectorDoorOpenControl>(
                    [&record](engine::Entity, SectorDoor& door,
                            SectorDoorOpenControl& control) {
                        if (door.enabled
                                && door.placedObjectId == record.doorId) {
                            ++control.navigationHolderCount;
                        }
                    });
        }
    }
    if (externalDoorHoldId > 0) {
        world.ForEach<SectorDoor, SectorDoorOpenControl>(
                [externalDoorHoldId](engine::Entity, SectorDoor& door,
                        SectorDoorOpenControl& control) {
                    if (door.enabled
                            && door.placedObjectId == externalDoorHoldId) {
                        ++control.navigationHolderCount;
                    }
                });
    }
}

void SynchronizeSectorNavigationDoorLinksSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const std::vector<SectorDynamicDoorCollider>& doorColliders)
{
    const SectorNavigationSettings& settings = navigation.Settings();
    for (const SectorNavigationDebugDoorLink& link :
            navigation.DebugCache().doorLinks) {
        bool found = false;
        bool enabled = false;
        uint32_t holderCount = 0;
        world.ForEach<SectorDoor>(
                [&link, &found, &enabled, &holderCount, &world](
                        engine::Entity entity, SectorDoor& door) {
                    if (door.placedObjectId != link.placedObjectId) return;
                    found = true;
                    enabled = door.enabled;
                    if (world.Has<SectorDoorOpenControl>(entity)) {
                        holderCount = world.Get<SectorDoorOpenControl>(entity)
                                .navigationHolderCount;
                    }
                });
        SectorNavigationDoorLinkState state =
                SectorNavigationDoorLinkState::Disabled;
        if (found && enabled) {
            state = SectorDoorTraversalIsClear(
                    link.placedObjectId,
                    link.frontStage,
                    link.backStage,
                    settings.agentRadius,
                    settings.agentHeight,
                    doorColliders)
                    ? SectorNavigationDoorLinkState::Clear
                    : SectorNavigationDoorLinkState::RequiresOpening;
        }
        navigation.SetDoorLinkRuntimeState(
                link.placedObjectId, state, holderCount);
    }
}

void CollectNpcDoorObstacles(
        engine::World& world,
        const NpcNavigationRuntime& runtime,
        std::vector<SectorDoorPlayerObstacle>& outObstacles,
        const SectorDoorPlayerObstacle* playerObstacle)
{
    outObstacles.clear();
    if (playerObstacle != nullptr) outObstacles.push_back(*playerObstacle);
    const SectorDoorPlayerObstacle npcShape{{}, 0.25f, 1.6f};
    for (const NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || !IsActive(record.phase)
                || (record.doorPhase != NpcDoorTraversalPhase::WaitingForClearance
                    && record.doorPhase != NpcDoorTraversalPhase::Crossing)
                || !world.IsAlive(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)) continue;
        SectorDoorPlayerObstacle obstacle = npcShape;
        obstacle.feetPosition = world.Get<SectorObjectTransform>(record.entity).position;
        outObstacles.push_back(obstacle);
    }
}

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
        float rawDt,
        const SectorDoorPlayerObstacle* playerObstacle,
        bool freezeAi)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    bool movedAnyNpc = false;
    const SectorNavigationSettings& navSettings = navigation.Settings();
    const SectorCollisionMoveConfig moveConfig{
            navSettings.agentRadius,
            navSettings.agentHeight,
            navSettings.agentMaximumClimb,
            4,
            true};

    for (NpcNavigationRecord& record : runtime.records) {
        record.footstepMovementActive = false;
        record.footstepEvent = false;
    }

    runtime.collisionCylinders.clear();
    for (const NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || !world.IsAlive(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)) {
            continue;
        }
        runtime.collisionCylinders.push_back({
                record.placedObjectId,
                world.Get<SectorObjectTransform>(record.entity).position,
                navSettings.agentRadius,
                navSettings.agentHeight});
    }

    // Submit every agent's preferred velocity before the single Crowd update.
    // Idle agents participate with zero velocity so moving agents avoid them.
    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied || !world.IsAlive(record.entity)
                || !world.Has<NpcRuntimeInstance>(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)) {
            continue;
        }
        if (navigation.State() == SectorNavigationState::Ready
                && !navigation.IsAgentRecordValid(record.agentHandle)) {
            record.agentHandle = navigation.AllocateAgentRecord();
        }
        if (!navigation.IsAgentRecordValid(record.agentHandle)) continue;
        const NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(record.entity);
        const SectorObjectTransform& transform =
                world.Get<SectorObjectTransform>(record.entity);
        const float authoredSpeed = record.gait == NpcMoveGait::Run
                ? npc.runSpeed : npc.walkSpeed;
        const float maximumSpeed = record.movementSpeedOverride > 0.0f
                ? record.movementSpeedOverride : authoredSpeed;
        Vector2 preferred{};
        const bool staggered = world.Has<NpcCombatState>(record.entity)
                && world.Get<NpcCombatState>(record.entity)
                        .staggerRemainingSeconds > 0.0f;
        const bool frozenAi = freezeAi
                && (record.authority == NpcMoveAuthority::Ai
                    || record.authority == NpcMoveAuthority::Patrol);
        if (!staggered && !frozenAi && IsActive(record.phase)
                && navigation.IsPathRecordValid(record.pathHandle)
                && !record.tileReplanPending
                && record.nextCorner < record.cornerCount
                && record.doorPhase != NpcDoorTraversalPhase::WaitingForClearance) {
            const Vector3 target = record.doorPhase == NpcDoorTraversalPhase::Crossing
                    ? record.doorLanding : record.corners[record.nextCorner];
            const Vector2 delta{
                    target.x - transform.position.x,
                    target.z - transform.position.z};
            const float length = Vector2Length(delta);
            if (length > MovementDistanceEpsilon) {
                preferred = Vector2Scale(delta, maximumSpeed / length);
            }
        }
        record.preferredVelocity = preferred;
        record.playerAvoidanceActive = false;
        Vector2 submitted = preferred;
        if (!npc.hostile && record.doorPhase != NpcDoorTraversalPhase::Crossing) {
            submitted = ApplyFriendlyPlayerAvoidance(
                    record,
                    transform.position,
                    preferred,
                    navSettings.agentRadius,
                    navSettings.agentHeight,
                    navSettings.agentRadius
                            * navigation.CrowdSettings()
                                    .collisionQueryRangeRadiusScale,
                    playerObstacle,
                    record.playerAvoidanceActive);
        }
        const bool participate = record.doorPhase != NpcDoorTraversalPhase::Crossing;
        record.crowdAttached = navigation.SynchronizeCrowdAgent(
                record.agentHandle,
                transform.position,
                record.actualVelocity,
                maximumSpeed,
                participate) && participate;
        if (record.crowdAttached) {
            navigation.SetCrowdAgentDesiredVelocity(record.agentHandle, submitted);
        }
    }
    navigation.UpdateCrowd(dt);

    for (NpcNavigationRecord& record : runtime.records) {
        if (!record.occupied) continue;
        if (!world.IsAlive(record.entity)
                || !world.Has<NpcRuntimeInstance>(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)
                || !world.Has<SectorObject>(record.entity)) {
            ReleasePath(navigation, record, &world);
            if (!IsNull(record.agentHandle)) {
                navigation.ReleaseAgentRecord(record.agentHandle);
            }
            record.occupied = false;
            continue;
        }

        NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(record.entity);
        SectorObjectTransform& transform = world.Get<SectorObjectTransform>(record.entity);
        SectorObject& object = world.Get<SectorObject>(record.entity);
        SectorObjectVisualOffset* visualOffset =
                world.Has<SectorObjectVisualOffset>(record.entity)
                ? &world.Get<SectorObjectVisualOffset>(record.entity) : nullptr;
        record.actualVelocity = {};
        record.replanCooldownSeconds = std::max(
                0.0f, record.replanCooldownSeconds - dt);
        bool capturedStepOffset = false;
        float resolvedHorizontalDistance = 0.0f;

        if (navigation.State() == SectorNavigationState::Ready
                && !navigation.IsAgentRecordValid(record.agentHandle)) {
            record.agentHandle = navigation.AllocateAgentRecord();
            if (IsNull(record.agentHandle)) {
                ++runtime.counters.capacityWarnings;
                if (IsActive(record.phase)) {
                    npc.action = NpcAction::Idle;
                    SetTerminal(
                            world,
                            navigation,
                            runtime,
                            record,
                            NpcMovePhase::Failed,
                            SectorNavigationQueryStatus::CapacityExceeded,
                            "navigation agent capacity was exceeded after rebuild");
                }
            }
        }

        if (IsActive(record.phase)
                && navigation.State() != SectorNavigationState::Ready) {
            npc.action = NpcAction::Idle;
            SetTerminal(
                    world,
                    navigation,
                    runtime,
                    record,
                    NpcMovePhase::Failed,
                    SectorNavigationQueryStatus::NavigationUnavailable,
                    "navigation became unavailable during movement");
        }

        if (IsActive(record.phase)
                && !navigation.IsPathRecordValid(record.pathHandle)) {
            npc.action = NpcAction::Idle;
            SetTerminal(
                    world,
                    navigation,
                    runtime,
                    record,
                    NpcMovePhase::Failed,
                    SectorNavigationQueryStatus::NavigationUnavailable,
                    "navigation was rebuilt during movement");
        }

        if (IsActive(record.phase)
                && navigation.CorridorTouchesChangedTile(
                        record.corridorTiles.data(),
                        record.corridorTileCount,
                        record.pathTileRevision)) {
            record.tileReplanPending = true;
            record.desiredVelocity = {};
            npc.action = NpcAction::Idle;
            SetDiagnostic(record, "corridor tile changed; replan pending");
        }
        if (IsActive(record.phase) && record.tileReplanPending
                && record.replanCooldownSeconds <= 0.0f) {
            if (record.replanCount >= MaximumReplans
                    || !Replan(world, navigation, runtime, record)) {
                ++runtime.counters.stalls;
                SetTerminal(
                        world,
                        navigation,
                        runtime,
                        record,
                        NpcMovePhase::Failed,
                        record.lastQueryStatus,
                        "dynamic obstacle invalidated the route and no replacement path was available");
            }
        }

        const bool staggered = world.Has<NpcCombatState>(record.entity)
                && world.Get<NpcCombatState>(record.entity)
                        .staggerRemainingSeconds > 0.0f;
        const bool dead = (world.Has<NpcCombatState>(record.entity)
                        && world.Get<NpcCombatState>(record.entity).dead)
                || (world.Has<Health>(record.entity)
                        && IsDepleted(world.Get<Health>(record.entity)));
        const bool frozenAi = freezeAi
                && (record.authority == NpcMoveAuthority::Ai
                    || record.authority == NpcMoveAuthority::Patrol);
        if (!staggered && !dead && !freezeAi && dt > 0.0f
                && world.Has<NpcAiState>(record.entity)) {
            const NpcAiState& ai = world.Get<NpcAiState>(record.entity);
            const float advanceFactor = ai.attackCommitted
                    ? NpcAiAttackAdvanceSpeedFactor(
                            ai.attackPhase,
                            ai.attack.hitPhase,
                            ai.attack.advanceSpeedMultiplier)
                    : 0.0f;
            if (advanceFactor > 0.0f) {
                const Vector2 direction{
                        std::sin(transform.yawRadians),
                        std::cos(transform.yawRadians)};
                const Vector2 desiredDelta = Vector2Scale(
                        direction, npc.runSpeed * advanceFactor * dt);
                const SectorCollisionMoveState moveState{
                        {transform.position.x, transform.position.z},
                        transform.position.y,
                        object.currentSectorId,
                        true};
                const SectorCollisionMoveResult result =
                        ResolveNpcHorizontalMovement(
                                collisionWorld,
                                doorColliders,
                                staticColliders,
                                map,
                                runtime,
                                record,
                                playerObstacle,
                                moveConfig,
                                moveState,
                                desiredDelta);
                const Vector2 actual = ApplyNpcMovementResult(
                        collisionWorld,
                        staticColliders,
                        runtime,
                        record,
                        moveConfig,
                        result,
                        transform,
                        object,
                        visualOffset,
                        capturedStepOffset);
                record.actualVelocity = Vector2Scale(actual, 1.0f / dt);
                if (Vector2Length(actual) > MovementDistanceEpsilon) {
                    movedAnyNpc = true;
                }
            }
        }
        if (!staggered && !frozenAi && IsActive(record.phase)
                && !record.tileReplanPending && dt > 0.0f) {
            const float authoredMovementSpeed = record.gait == NpcMoveGait::Run
                    ? npc.runSpeed : npc.walkSpeed;
            const float movementSpeed = record.movementSpeedOverride > 0.0f
                    ? record.movementSpeedOverride : authoredMovementSpeed;
            const SectorNavigationCrowdAgentState crowdState =
                    navigation.GetCrowdAgentState(record.agentHandle);
            record.crowdAttached = crowdState.attached;
            record.crowdNeighborCount = crowdState.neighborCount;
            record.crowdNearestNeighborDistance =
                    crowdState.nearestNeighborDistance;
            Vector2 steeringVelocity = crowdState.attached
                    ? crowdState.steeredVelocity : record.preferredVelocity;
            if (record.steeringRecoveryActive
                    || record.doorPhase == NpcDoorTraversalPhase::Crossing) {
                steeringVelocity = record.preferredVelocity;
            }
            record.desiredVelocity = steeringVelocity;
            const float steeringSpeed = Vector2Length(steeringVelocity);
            float movementBudget = steeringSpeed * dt;
            Vector2 totalActual{};
            const size_t cornerBeforeMovement = record.nextCorner;
            int stepCount = 0;
            while (movementBudget > MovementDistanceEpsilon
                    && record.nextCorner < record.cornerCount
                    && stepCount++ < 64) {
                const Vector3 corner = record.doorPhase == NpcDoorTraversalPhase::Crossing
                        ? record.doorLanding
                        : record.corners[record.nextCorner];
                Vector2 toCorner{
                        corner.x - transform.position.x,
                        corner.z - transform.position.z};
                float distance = Vector2Length(toCorner);
                if (distance <= ArrivalTolerance) {
                    if (record.doorPhase == NpcDoorTraversalPhase::Approaching
                            || record.doorPhase
                                    == NpcDoorTraversalPhase::WaitingForClearance) {
                        break;
                    }
                    ++record.nextCorner;
                    if (record.doorPhase == NpcDoorTraversalPhase::Crossing) {
                        record.doorPhase = NpcDoorTraversalPhase::None;
                        record.doorId = 0;
                        record.doorDirection = SectorNavigationDoorDirection::None;
                        record.doorLanding = {};
                        record.doorWaitSeconds = 0.0f;
                        record.holdsDoor = false;
                        SetDiagnostic(record, "door traversal completed");
                    }
                    continue;
                }
                Vector2 direction = Vector2Scale(toCorner, 1.0f / distance);
                if (record.doorPhase != NpcDoorTraversalPhase::Crossing
                        && steeringSpeed > MovementDistanceEpsilon) {
                    direction = Vector2Scale(
                            steeringVelocity, 1.0f / steeringSpeed);
                }
                const float requestedDistance = std::min({
                        distance,
                        movementBudget,
                        navSettings.agentRadius});
                const Vector2 desiredDelta = Vector2Scale(direction, requestedDistance);
                const SectorCollisionMoveState moveState{
                        {transform.position.x, transform.position.z},
                        transform.position.y,
                        object.currentSectorId,
                        true};
                const SectorCollisionMoveResult result =
                        ResolveNpcHorizontalMovement(
                                collisionWorld,
                                doorColliders,
                                staticColliders,
                                map,
                                runtime,
                                record,
                                playerObstacle,
                                moveConfig,
                                moveState,
                                desiredDelta);
                const Vector2 actual = ApplyNpcMovementResult(
                        collisionWorld,
                        staticColliders,
                        runtime,
                        record,
                        moveConfig,
                        result,
                        transform,
                        object,
                        visualOffset,
                        capturedStepOffset);
                totalActual = Vector2Add(totalActual, actual);
                const float actualDistance = Vector2Length(actual);
                movementBudget -= requestedDistance;
                if (actualDistance <= MovementDistanceEpsilon) break;
                movedAnyNpc = true;
                toCorner = {
                        corner.x - transform.position.x,
                        corner.z - transform.position.z};
                if (Vector2Length(toCorner) <= ArrivalTolerance) {
                    if (record.doorPhase == NpcDoorTraversalPhase::Crossing) {
                        ++record.nextCorner;
                        record.doorPhase = NpcDoorTraversalPhase::None;
                        record.doorId = 0;
                        record.doorDirection = SectorNavigationDoorDirection::None;
                        record.doorLanding = {};
                        record.doorWaitSeconds = 0.0f;
                        record.holdsDoor = false;
                        SetDiagnostic(record, "door traversal completed");
                    } else if (record.doorPhase == NpcDoorTraversalPhase::None) {
                        ++record.nextCorner;
                    }
                }
            }

            const float actualDistance = Vector2Length(totalActual);
            resolvedHorizontalDistance = actualDistance;
            record.actualVelocity = dt > 0.0f
                    ? Vector2Scale(totalActual, 1.0f / dt) : Vector2{};
            if (actualDistance > MovementDistanceEpsilon) {
                const Vector2 facingDirection =
                        ResolveNpcLocomotionFacingDirection(
                                record.preferredVelocity,
                                record.actualVelocity);
                if (Vector2LengthSqr(facingDirection)
                        > MovementDistanceEpsilon) {
                    const float targetYaw = std::atan2(
                            facingDirection.x, facingDirection.y);
                    const float delta = ShortestAngleDelta(
                            transform.yawRadians, targetYaw);
                    const float maximumTurn = TurnRateRadiansPerSecond * dt;
                    transform.yawRadians += std::clamp(
                            delta, -maximumTurn, maximumTurn);
                }
                npc.action = record.gait == NpcMoveGait::Run
                        ? NpcAction::Run : NpcAction::Walk;
            } else if (record.doorPhase
                    == NpcDoorTraversalPhase::WaitingForClearance) {
                npc.action = NpcAction::Idle;
            } else {
                npc.action = NpcAction::Idle;
            }

            if (record.doorPhase == NpcDoorTraversalPhase::WaitingForClearance) {
                ResetProgressTracking(record);
            } else if (record.nextCorner != cornerBeforeMovement) {
                ResetProgressTracking(record);
            } else if (record.nextCorner < record.cornerCount) {
                const Vector3 progressTarget =
                        record.doorPhase == NpcDoorTraversalPhase::Crossing
                        ? record.doorLanding
                        : record.corners[record.nextCorner];
                const Vector2 progressDelta{
                        progressTarget.x - transform.position.x,
                        progressTarget.z - transform.position.z};
                const float cornerDistance = Vector2Length(progressDelta);
                const bool recoveringBefore = record.steeringRecoveryActive;
                UpdateNpcWaypointProgressTracking(
                        record,
                        record.nextCorner,
                        cornerDistance,
                        navSettings.agentRadius,
                        movementSpeed,
                        dt);
                if (!recoveringBefore && record.steeringRecoveryActive) {
                    SetDiagnostic(record,
                            "direct steering recovery after no waypoint progress");
                }
            }

            const float destinationDx =
                    record.projectedDestination.x - transform.position.x;
            const float destinationDz =
                    record.projectedDestination.z - transform.position.z;
            if (record.nextCorner >= record.cornerCount
                    && std::sqrt(destinationDx * destinationDx
                                 + destinationDz * destinationDz)
                            <= ArrivalTolerance) {
                npc.action = NpcAction::Idle;
                SetTerminal(
                        world,
                        navigation,
                        runtime,
                        record,
                        NpcMovePhase::Arrived,
                        SectorNavigationQueryStatus::Success,
                        "arrived within 0.10m tolerance");
            } else if (record.stallSeconds >= ReplanTriggerSeconds
                    && record.replanCooldownSeconds <= 0.0f) {
                if (record.replanCount >= MaximumReplans
                        || !Replan(world, navigation, runtime, record)) {
                    npc.action = NpcAction::Idle;
                    ++runtime.counters.stalls;
                    SetTerminal(
                            world,
                            navigation,
                            runtime,
                            record,
                            NpcMovePhase::Failed,
                            SectorNavigationQueryStatus::Stalled,
                            "movement stalled after bounded replans");
                }
            }

            record.driftCheckSeconds += dt;
            if (IsActive(record.phase)
                    && record.driftCheckSeconds >= DriftCheckIntervalSeconds) {
                record.driftCheckSeconds = 0.0f;
                const SectorNavigationNearestPointResult nearest =
                        navigation.FindNearestPoint(transform.position);
                const float driftThreshold = std::max(
                        0.5f, navSettings.agentRadius * 2.0f);
                const float dx = nearest.nearestPosition.x - transform.position.x;
                const float dz = nearest.nearestPosition.z - transform.position.z;
                if (nearest.status != SectorNavigationQueryStatus::Success
                        || std::sqrt(dx * dx + dz * dz) > driftThreshold) {
                    if (record.replanCooldownSeconds <= 0.0f
                            && (record.replanCount >= MaximumReplans
                                || !Replan(world, navigation, runtime, record))) {
                        npc.action = NpcAction::Idle;
                        SetTerminal(
                                world,
                                navigation,
                                runtime,
                                record,
                                NpcMovePhase::Failed,
                                SectorNavigationQueryStatus::Stalled,
                                "agent drifted off the navigation surface");
                    }
                }
            }
        } else if (staggered) {
            record.preferredVelocity = {};
            record.desiredVelocity = {};
            record.actualVelocity = {};
            npc.action = NpcAction::Idle;
        } else if (!IsActive(record.phase) && !npc.actionLockedByAi) {
            npc.action = NpcAction::Idle;
        }

        if (visualOffset != nullptr && !capturedStepOffset
                && visualOffset->position.y != 0.0f && dt > 0.0f) {
            visualOffset->position.y *= std::exp(-VisualStepSmoothingRate * dt);
            if (std::fabs(visualOffset->position.y) < VisualOffsetEpsilon) {
                visualOffset->position.y = 0.0f;
            }
        }

        UpdateNpcFootstepCadence(
                record,
                IsActive(record.phase)
                        || resolvedHorizontalDistance > MovementDistanceEpsilon,
                resolvedHorizontalDistance);

        if (world.Has<SectorDynamicModel>(record.entity)) {
            SectorDynamicModel& model = world.Get<SectorDynamicModel>(record.entity);
            model.containingSectorAmbient = ComputeSectorModelAmbient(
                    map, object.currentSectorId);
            model.environmentExposure = ComputeSectorModelEnvironmentExposure(
                    map, object.currentSectorId);
        }
        record.physicalPosition = transform.position;
        record.visualPosition = transform.position;
        if (visualOffset != nullptr) {
            record.visualPosition = Vector3Add(
                    record.visualPosition, visualOffset->position);
        }
    }

    if (movedAnyNpc) {
        UpdateSectorObjectBakedLightingSystem(
                world, objectLightProbes, &map);
    }
    UpdateNpcAnimationStateSystem(world, assets, definitions);
}

} // namespace game
