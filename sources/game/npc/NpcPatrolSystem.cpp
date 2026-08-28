#include "game/npc/NpcPatrolSystem.h"

#include "engine/ecs/World.h"
#include "game/Health.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;
constexpr float LookTurnRate = 1.4f;
constexpr float WaypointTurnRate = TwoPi;
constexpr float DestinationEpsilon = 0.02f;
constexpr float RetryDelaySeconds = 0.5f;

bool ParticipantLess(const NpcPatrolParticipant& a, const NpcPatrolParticipant& b)
{
    if (a.patrolEditorId != b.patrolEditorId) {
        return a.patrolEditorId < b.patrolEditorId;
    }
    if (a.waypointIndex != b.waypointIndex) return a.waypointIndex < b.waypointIndex;
    if (a.claimsSlot != b.claimsSlot) return a.claimsSlot > b.claimsSlot;
    return a.placedObjectId < b.placedObjectId;
}

bool SameGroup(const NpcPatrolParticipant& a, const NpcPatrolParticipant& b)
{
    return a.patrolEditorId == b.patrolEditorId
            && a.waypointIndex == b.waypointIndex;
}

float ShortestAngleDelta(float from, float to)
{
    float delta = std::fmod(to - from, TwoPi);
    if (delta > Pi) delta -= TwoPi;
    if (delta < -Pi) delta += TwoPi;
    return delta;
}

bool TurnTowardWaypointYaw(float& yawRadians, float targetYawRadians, float dt)
{
    const float delta = ShortestAngleDelta(yawRadians, targetYawRadians);
    const float maximumTurn = WaypointTurnRate * dt;
    if (std::abs(delta) <= maximumTurn) {
        yawRadians += delta;
        return true;
    }
    yawRadians += std::clamp(delta, -maximumTurn, maximumTurn);
    return false;
}

Vector2 ResolveDestination(
        const SectorNavigationWorld& navigation,
        const SectorCompiledLevelMarker& marker,
        size_t slotIndex,
        size_t groupSize)
{
    const Vector3 markerWorld = SectorAuthoringToWorldPosition(marker.position);
    if (groupSize <= 1) return {markerWorld.x, markerWorld.z};
    constexpr size_t slotsPerRing = 8;
    const size_t ring = slotIndex / slotsPerRing;
    const size_t indexOnRing = slotIndex % slotsPerRing;
    const size_t slotsOnRing = std::min(
            slotsPerRing, groupSize - ring * slotsPerRing);
    const float radius = navigation.Settings().agentRadius
            * 2.4f * static_cast<float>(ring + 1);
    const float angle = marker.yawRadians
            + TwoPi * static_cast<float>(indexOnRing)
                    / static_cast<float>(slotsOnRing);
    const Vector3 candidate{
            markerWorld.x + std::sin(angle) * radius,
            markerWorld.y,
            markerWorld.z + std::cos(angle) * radius};
    const SectorNavigationNearestPointResult projected =
            navigation.FindNearestPoint(candidate);
    if (projected.status == SectorNavigationQueryStatus::Success) {
        return {projected.nearestPosition.x, projected.nearestPosition.z};
    }
    return {candidate.x, candidate.z};
}

} // namespace

void NotifyNpcPatrolScriptMoveStarted(
        engine::World& world,
        const NpcNavigationRuntime& navigationRuntime,
        std::string_view instanceId)
{
    for (const NpcNavigationRecord& record : navigationRuntime.records) {
        if (!record.occupied || record.instanceId != instanceId
                || !world.IsAlive(record.entity)
                || !world.Has<NpcPatrolState>(record.entity)) continue;
        NpcPatrolState& state = world.Get<NpcPatrolState>(record.entity);
        state.scriptOverrideActive = true;
        if (state.scriptMoveStopsPatrol) {
            state.stoppedByScript = true;
            state.phase = NpcPatrolPhase::StoppedByScript;
        } else {
            state.resumePhase = state.phase;
            state.phase = NpcPatrolPhase::SuspendedScript;
        }
        return;
    }
}

void InitializeNpcPatrolRuntime(NpcPatrolRuntime& runtime, size_t capacity)
{
    runtime.participants.clear();
    runtime.participants.reserve(capacity);
    runtime.growthWarned = false;
}

void ClearNpcPatrolRuntime(NpcPatrolRuntime& runtime)
{
    const size_t capacity = runtime.participants.capacity();
    runtime.participants.clear();
    if (capacity > 0) runtime.participants.reserve(capacity);
    runtime.growthWarned = false;
}

void UpdateNpcPatrolSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& navigationRuntime,
        NpcPatrolRuntime& runtime,
        const SectorTopologyMap& map,
        float rawDt,
        bool frozen)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    runtime.participants.clear();
    world.ForEach<NpcPatrolState, SectorDynamicModel>(
            [&runtime](engine::Entity entity, NpcPatrolState& state,
                    SectorDynamicModel& model) {
                if (state.phase == NpcPatrolPhase::Complete
                        || state.phase == NpcPatrolPhase::StoppedByScript) return;
                if (runtime.participants.size() == runtime.participants.capacity()
                        && !runtime.growthWarned) {
                    runtime.growthWarned = true;
                    std::fprintf(stderr,
                            "[NPC Patrol WARNING] Participant capacity exceeded; runtime allocation may occur.\n");
                }
                runtime.participants.push_back({
                        entity, model.placedObjectId,
                        state.patrolEditorId, state.waypointIndex,
                        state.phase == NpcPatrolPhase::Moving
                                || state.phase == NpcPatrolPhase::Failed});
            });
    std::sort(runtime.participants.begin(), runtime.participants.end(), ParticipantLess);

    size_t groupBegin = 0;
    while (groupBegin < runtime.participants.size()) {
        size_t groupEnd = groupBegin + 1;
        while (groupEnd < runtime.participants.size()
                && SameGroup(runtime.participants[groupBegin],
                        runtime.participants[groupEnd])) ++groupEnd;
        size_t slotGroupSize = 0;
        while (groupBegin + slotGroupSize < groupEnd
                && runtime.participants[groupBegin + slotGroupSize].claimsSlot) {
            ++slotGroupSize;
        }
        for (size_t participantIndex = groupBegin;
                participantIndex < groupEnd; ++participantIndex) {
            const NpcPatrolParticipant& participant =
                    runtime.participants[participantIndex];
            if (!world.IsAlive(participant.entity)
                    || !world.Has<NpcPatrolState>(participant.entity)
                    || !world.Has<NpcRuntimeInstance>(participant.entity)
                    || !world.Has<SectorObjectTransform>(participant.entity)) continue;
            NpcPatrolState& state = world.Get<NpcPatrolState>(participant.entity);
            NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(participant.entity);
            SectorObjectTransform& transform =
                    world.Get<SectorObjectTransform>(participant.entity);
            const SectorCompiledPatrol* patrol =
                    FindSectorCompiledPatrol(map, state.patrolEditorId);
            if (patrol == nullptr || patrol->waypoints.empty()
                    || state.waypointIndex >= patrol->waypoints.size()) {
                state.phase = NpcPatrolPhase::Failed;
                npc.action = NpcAction::Idle;
                continue;
            }
            const bool dead = (world.Has<NpcCombatState>(participant.entity)
                            && world.Get<NpcCombatState>(participant.entity).dead)
                    || (world.Has<Health>(participant.entity)
                            && IsDepleted(world.Get<Health>(participant.entity)));
            if (dead) {
                CancelNpcMoveForEntity(
                        world, navigation, navigationRuntime,
                        participant.entity, state.requestId);
                state.phase = NpcPatrolPhase::Complete;
                continue;
            }

            const NpcMoveStatus move = GetNpcMoveStatusForEntity(
                    navigationRuntime, participant.entity);
            const bool aiOwnsMovement = move.found
                    && move.phase == NpcMovePhase::FollowingPath
                    && move.authority == NpcMoveAuthority::Ai;
            const bool aware = world.Has<NpcAiState>(participant.entity)
                    && world.Get<NpcAiState>(participant.entity).awareness
                            != NpcAwarenessState::Unaware;
            if (aware || aiOwnsMovement) {
                if (move.authority == NpcMoveAuthority::Patrol) {
                    CancelNpcMoveForEntity(
                            world, navigation, navigationRuntime,
                            participant.entity, state.requestId);
                }
                if (state.phase != NpcPatrolPhase::SuspendedAi) {
                    if (state.phase != NpcPatrolPhase::SuspendedScript) {
                        state.resumePhase = state.phase;
                    }
                    state.phase = NpcPatrolPhase::SuspendedAi;
                }
                continue;
            }
            if (state.phase == NpcPatrolPhase::SuspendedAi) {
                state.phase = state.resumePhase;
                if (state.phase == NpcPatrolPhase::Moving) state.requestId = 0;
            }

            const bool scriptMoving = move.found
                    && move.phase == NpcMovePhase::FollowingPath
                    && move.authority == NpcMoveAuthority::Script;
            if (scriptMoving) {
                state.scriptOverrideActive = true;
                if (state.scriptMoveStopsPatrol) {
                    state.stoppedByScript = true;
                    state.phase = NpcPatrolPhase::StoppedByScript;
                } else {
                    if (state.phase != NpcPatrolPhase::SuspendedScript) {
                        state.resumePhase = state.phase;
                    }
                    state.phase = NpcPatrolPhase::SuspendedScript;
                }
                continue;
            }
            if (state.scriptOverrideActive) {
                state.scriptOverrideActive = false;
                if (state.stoppedByScript) continue;
                state.phase = state.resumePhase;
                if (state.phase == NpcPatrolPhase::Moving) state.requestId = 0;
            }
            if (frozen || state.phase == NpcPatrolPhase::StoppedByScript
                    || state.phase == NpcPatrolPhase::Complete) continue;

            const SectorCompiledPatrolWaypoint& waypoint =
                    patrol->waypoints[state.waypointIndex];
            const SectorCompiledLevelMarker* marker =
                    FindSectorCompiledLevelMarkerByAuthoringId(
                            map, waypoint.sourceAuthoringMarkerId);
            if (marker == nullptr) {
                state.phase = NpcPatrolPhase::Failed;
                continue;
            }

            if (state.phase == NpcPatrolPhase::Turning) {
                npc.action = NpcAction::Idle;
                if (TurnTowardWaypointYaw(
                            transform.yawRadians,
                            state.waypointBaseYawRadians,
                            dt)) {
                    state.waypointBaseYawRadians = transform.yawRadians;
                    state.phase = NpcPatrolPhase::Waiting;
                    if (state.waitRemainingSeconds <= 0.0f) {
                        AdvanceNpcPatrolWaypoint(state, *patrol);
                    }
                }
                continue;
            }

            if (state.phase == NpcPatrolPhase::Waiting) {
                npc.action = NpcAction::Idle;
                state.waitRemainingSeconds = std::max(
                        0.0f, state.waitRemainingSeconds - dt);
                if (waypoint.lookAround && waypoint.lookArcDegrees > 0.0f) {
                    const float halfArc = waypoint.lookArcDegrees * Pi / 360.0f;
                    state.lookOffsetRadians += state.lookDirection * LookTurnRate * dt;
                    if (state.lookOffsetRadians >= halfArc) {
                        state.lookOffsetRadians = halfArc;
                        state.lookDirection = -1.0f;
                    } else if (state.lookOffsetRadians <= -halfArc) {
                        state.lookOffsetRadians = -halfArc;
                        state.lookDirection = 1.0f;
                    }
                    transform.yawRadians = state.waypointBaseYawRadians
                            + state.lookOffsetRadians;
                }
                if (state.waitRemainingSeconds <= 0.0f) {
                    AdvanceNpcPatrolWaypoint(state, *patrol);
                }
                continue;
            }

            size_t effectiveGroupSize = slotGroupSize;
            size_t effectiveSlotIndex = participantIndex - groupBegin;
            if (!participant.claimsSlot) {
                effectiveSlotIndex = slotGroupSize;
                ++effectiveGroupSize;
            }
            const int slotIndex = effectiveGroupSize > 1
                    ? static_cast<int>(effectiveSlotIndex) : -1;
            const Vector2 destination = ResolveDestination(
                    navigation, *marker,
                    effectiveSlotIndex, effectiveGroupSize);
            const bool destinationChanged = state.destinationInitialized
                    && Vector2Distance(state.destinationXZ, destination)
                            > DestinationEpsilon;
            if (destinationChanged && move.authority == NpcMoveAuthority::Patrol
                    && move.phase == NpcMovePhase::FollowingPath) {
                CancelNpcMoveForEntity(
                        world, navigation, navigationRuntime,
                        participant.entity, state.requestId);
                state.requestId = 0;
            }
            state.slotIndex = slotIndex;
            state.destinationXZ = destination;
            state.destinationInitialized = true;

            const NpcMoveStatus current = GetNpcMoveStatusForEntity(
                    navigationRuntime, participant.entity);
            if (current.found && current.authority == NpcMoveAuthority::Patrol
                    && current.requestId == state.requestId
                    && current.phase == NpcMovePhase::FollowingPath) continue;
            if (current.found && current.authority == NpcMoveAuthority::Patrol
                    && current.requestId == state.requestId
                    && current.phase == NpcMovePhase::Arrived) {
                state.waitRemainingSeconds =
                        static_cast<float>(waypoint.delayMilliseconds) / 1000.0f;
                state.waypointBaseYawRadians = patrol->faceWaypointOrientation
                        ? marker->yawRadians : transform.yawRadians;
                state.lookOffsetRadians = 0.0f;
                state.lookDirection = 1.0f;
                if (patrol->faceWaypointOrientation) {
                    state.phase = TurnTowardWaypointYaw(
                            transform.yawRadians,
                            state.waypointBaseYawRadians,
                            0.0f)
                            ? NpcPatrolPhase::Waiting
                            : NpcPatrolPhase::Turning;
                    if (state.phase == NpcPatrolPhase::Waiting) {
                        state.waypointBaseYawRadians = transform.yawRadians;
                    }
                } else {
                    state.phase = NpcPatrolPhase::Waiting;
                }
                if (state.phase == NpcPatrolPhase::Waiting
                        && state.waitRemainingSeconds <= 0.0f) {
                    AdvanceNpcPatrolWaypoint(state, *patrol);
                }
                continue;
            }
            state.retryRemainingSeconds = std::max(
                    0.0f, state.retryRemainingSeconds - dt);
            if (state.retryRemainingSeconds > 0.0f) continue;
            const NpcMoveRequestResult request = RequestNpcMoveForEntity(
                    world, navigation, collisionWorld, navigationRuntime,
                    participant.entity, destination,
                    waypoint.gait == SectorPatrolGait::Run
                            ? NpcMoveGait::Run : NpcMoveGait::Walk,
                    NpcMoveAuthority::Patrol);
            if (request.accepted) {
                state.requestId = request.requestId;
                state.phase = NpcPatrolPhase::Moving;
            } else {
                state.phase = NpcPatrolPhase::Failed;
                state.retryRemainingSeconds = RetryDelaySeconds;
            }
        }
        groupBegin = groupEnd;
    }
}

} // namespace game
