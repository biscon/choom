#include "game/npc/NpcNavigationSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/navigation/SectorNavigationWorld.h"
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
constexpr float StallTriggerSeconds = 0.75f;
constexpr float ReplanCooldownSeconds = 0.50f;
constexpr float DriftCheckIntervalSeconds = 0.25f;
constexpr uint32_t MaximumReplans = 3;
constexpr float TurnRateRadiansPerSecond = 12.566370614359172f;
constexpr float VisualStepSmoothingRate = 16.0f;
constexpr float VisualOffsetEpsilon = 0.0001f;
constexpr float PlayerAvoidancePredictionSeconds = 1.25f;
constexpr float PlayerAvoidancePadding = 0.15f;

size_t ActionIndex(NpcAction action)
{
    const size_t index = static_cast<size_t>(action);
    return index < kNpcActionCount ? index : 0;
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

bool IsActive(NpcMovePhase phase)
{
    return phase == NpcMovePhase::FollowingPath;
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
    record.stallSeconds = 0.0f;
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
        NpcNavigationRecord& record)
{
    if (!world.IsAlive(record.entity)
            || !world.Has<NpcRuntimeInstance>(record.entity)
            || !world.Has<NpcAnimationState>(record.entity)
            || !world.Has<engine::AnimatedModelInstance>(record.entity)
            || !world.Has<engine::AnimatedModelAnimator>(record.entity)) {
        return;
    }
    NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(record.entity);
    NpcAnimationState& state = world.Get<NpcAnimationState>(record.entity);
    engine::AnimatedModelInstance& instance =
            world.Get<engine::AnimatedModelInstance>(record.entity);
    engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(record.entity);
    const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
    const NpcDefinition* definition = FindNpcDefinition(definitions, npc.definitionId);
    if (!state.resolved) {
        if (asset == nullptr || definition == nullptr) return;
        state.missingAnimationMask = 0;
        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            const size_t index = ActionIndex(metadata.action);
            const NpcActionDefinition& action = GetNpcAction(*definition, metadata.action);
            state.animationSpeeds[index] = action.animationSpeed;
            state.animationIndices[index] = engine::FindModelAnimationIndex(
                    *asset, action.animation.c_str());
            if (state.animationIndices[index] == engine::InvalidModelAnimationIndex) {
                state.missingAnimationMask |= static_cast<uint8_t>(1u << index);
                std::fprintf(
                        stderr,
                        "[NPC WARNING] NPC '%s' semantic action '%s' has no model animation '%s'.\n",
                        npc.instanceId.c_str(),
                        metadata.displayName,
                        action.animation.c_str());
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

    if (ApplyNpcSemanticAnimation(state, animator, npc.action)
            == NpcAnimationApplyResult::Missing) {
        SetDiagnostic(record, "semantic action animation is missing; movement continues");
    }
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
    record.stallSeconds = 0.0f;
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
    state.appliedAction = requested;
    return NpcAnimationApplyResult::Applied;
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

NpcMoveRequestResult RequestNpcMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority)
{
    if (!IsValidNpcInstanceId(instanceId)) {
        return FailRequest(SectorNavigationQueryStatus::InvalidAgent, "invalid NPC instance ID");
    }
    NpcNavigationRecord* record = FindRecord(runtime, instanceId);
    if (record == nullptr || !world.IsAlive(record->entity)
            || !world.Has<SectorObjectTransform>(record->entity)
            || !world.Has<SectorObject>(record->entity)) {
        return FailRequest(SectorNavigationQueryStatus::InvalidAgent, "NPC instance was not found");
    }
    if (IsActive(record->phase)) {
        if (record->authority == NpcMoveAuthority::Script) {
            return FailRequest(
                    SectorNavigationQueryStatus::InvalidAgent,
                    authority == NpcMoveAuthority::Script
                            ? "NPC already has an active scripted move"
                            : "NPC movement is controlled by a script");
        }
        if (authority != NpcMoveAuthority::Script
                || record->authority != NpcMoveAuthority::Ai) {
            return FailRequest(
                    SectorNavigationQueryStatus::InvalidAgent,
                    "NPC already has an active move");
        }
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
    record->authority = authority;
    record->requestId = AllocateRequestId(runtime);
    record->requestedDestinationXZ = destinationXZ;
    record->projectedDestination = path.projectedDestination;
    record->desiredVelocity = {};
    record->actualVelocity = {};
    record->stallSeconds = 0.0f;
    record->replanCooldownSeconds = 0.0f;
    record->driftCheckSeconds = 0.0f;
    record->replanCount = 0;
    SetDiagnostic(*record, "following complete path");
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

NpcMoveStatus GetNpcMoveStatus(
        const NpcNavigationRuntime& runtime,
        std::string_view instanceId)
{
    NpcMoveStatus status;
    const NpcNavigationRecord* record = FindRecord(runtime, instanceId);
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
    status.message = record->diagnostic.data();
    return status;
}

void PrepareNpcDoorTraversalAndHoldsSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        float rawDt)
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
        const SectorDoorPlayerObstacle* playerObstacle)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    bool movedAnyNpc = false;
    const SectorNavigationSettings& navSettings = navigation.Settings();
    const SectorCollisionMoveConfig moveConfig{
            navSettings.agentRadius,
            navSettings.agentHeight,
            navSettings.agentMaximumClimb,
            4};

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
        const float maximumSpeed = record.gait == NpcMoveGait::Run
                ? npc.runSpeed : npc.walkSpeed;
        Vector2 preferred{};
        if (IsActive(record.phase) && !record.tileReplanPending
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

        if (IsActive(record.phase) && !record.tileReplanPending && dt > 0.0f) {
            const float movementSpeed = record.gait == NpcMoveGait::Run
                    ? npc.runSpeed : npc.walkSpeed;
            const SectorNavigationCrowdAgentState crowdState =
                    navigation.GetCrowdAgentState(record.agentHandle);
            record.crowdAttached = crowdState.attached;
            record.crowdNeighborCount = crowdState.neighborCount;
            record.crowdNearestNeighborDistance =
                    crowdState.nearestNeighborDistance;
            Vector2 steeringVelocity = crowdState.attached
                    ? crowdState.steeredVelocity : record.preferredVelocity;
            if (record.doorPhase == NpcDoorTraversalPhase::Crossing) {
                steeringVelocity = record.preferredVelocity;
            }
            record.desiredVelocity = steeringVelocity;
            const float steeringSpeed = Vector2Length(steeringVelocity);
            float movementBudget = steeringSpeed * dt;
            Vector2 totalActual{};
            Vector2 progressDirection{};
            if (record.nextCorner < record.cornerCount) {
                const Vector3 progressTarget =
                        record.doorPhase == NpcDoorTraversalPhase::Crossing
                        ? record.doorLanding
                        : record.corners[record.nextCorner];
                progressDirection = {
                        progressTarget.x - transform.position.x,
                        progressTarget.z - transform.position.z};
                const float progressLength = Vector2Length(progressDirection);
                if (progressLength > MovementDistanceEpsilon) {
                    progressDirection = Vector2Scale(
                            progressDirection, 1.0f / progressLength);
                }
            }
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
                result = ResolveNpcCollisionCylindersForMovement(
                        moveState,
                        result,
                        moveConfig,
                        record.placedObjectId,
                        runtime.collisionCylinders.data(),
                        runtime.collisionCylinders.size());
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
                    if (std::fabs(floorDelta)
                            <= moveConfig.stepHeight + 0.001f) {
                        transform.position.y = support.floorZ;
                        if (visualOffset != nullptr
                                && std::fabs(transform.position.y - previousPhysicalY)
                                        > 0.001f) {
                            visualOffset->position.y = previousVisualY - transform.position.y;
                            capturedStepOffset = true;
                        }
                    }
                }
                for (NpcCollisionCylinder& cylinder :
                        runtime.collisionCylinders) {
                    if (cylinder.stableId == record.placedObjectId) {
                        cylinder.feetPosition = transform.position;
                        break;
                    }
                }

                const Vector2 actual{
                        transform.position.x - previousXZ.x,
                        transform.position.z - previousXZ.y};
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
            record.actualVelocity = dt > 0.0f
                    ? Vector2Scale(totalActual, 1.0f / dt) : Vector2{};
            const float forwardProgress = Vector2DotProduct(
                    totalActual, progressDirection);
            if (actualDistance > MovementDistanceEpsilon) {
                if (forwardProgress > MovementDistanceEpsilon) {
                    record.stallSeconds = 0.0f;
                } else {
                    record.stallSeconds += dt;
                }
                const float targetYaw = std::atan2(totalActual.x, totalActual.y);
                const float delta = ShortestAngleDelta(transform.yawRadians, targetYaw);
                const float maximumTurn = TurnRateRadiansPerSecond * dt;
                transform.yawRadians += std::clamp(delta, -maximumTurn, maximumTurn);
                npc.action = record.gait == NpcMoveGait::Run
                        ? NpcAction::Run : NpcAction::Walk;
            } else if (record.doorPhase
                    == NpcDoorTraversalPhase::WaitingForClearance) {
                record.stallSeconds = 0.0f;
                npc.action = NpcAction::Idle;
            } else {
                record.stallSeconds += dt;
                npc.action = NpcAction::Idle;
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
            } else if (record.stallSeconds >= StallTriggerSeconds
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
        } else if (!IsActive(record.phase)) {
            npc.action = NpcAction::Idle;
        }

        if (visualOffset != nullptr && !capturedStepOffset
                && visualOffset->position.y != 0.0f && dt > 0.0f) {
            visualOffset->position.y *= std::exp(-VisualStepSmoothingRate * dt);
            if (std::fabs(visualOffset->position.y) < VisualOffsetEpsilon) {
                visualOffset->position.y = 0.0f;
            }
        }

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
        ResolveNpcAnimations(world, assets, definitions, record);
    }

    if (movedAnyNpc) {
        UpdateSectorObjectBakedLightingSystem(
                world, objectLightProbes, &map);
    }
}

} // namespace game
