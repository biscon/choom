#include "game/npc/NpcRuntime.h"
#include "game/npc/NpcPatrolSystem.h"

#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>

namespace game {
namespace {

uint32_t NextPatrolRandom(uint32_t& state)
{
    if (state == 0) state = 0x6d2b79f5u;
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

void BuildShuffleOrder(NpcPatrolState& state, size_t waypointCount)
{
    state.shuffleOrder.clear();
    for (size_t index = 0; index < waypointCount; ++index) {
        if (index != state.waypointIndex) state.shuffleOrder.push_back(index);
    }
    for (size_t count = state.shuffleOrder.size(); count > 1; --count) {
        const size_t selected = static_cast<size_t>(
                NextPatrolRandom(state.randomState)) % count;
        std::swap(state.shuffleOrder[count - 1], state.shuffleOrder[selected]);
    }
    state.shuffleCursor = 0;
}

} // namespace

void InitializeNpcPatrolTraversal(
        NpcPatrolState& state,
        const SectorCompiledPatrol& patrol,
        bool randomStart,
        bool reverse,
        uint32_t randomSeed)
{
    const size_t count = patrol.waypoints.size();
    state.randomState = randomSeed == 0 ? 0x6d2b79f5u : randomSeed;
    state.direction = reverse && !patrol.shuffleWaypoints ? -1 : 1;
    state.shuffleOrder.clear();
    state.shuffleOrder.reserve(count > 0 ? count - 1 : 0);
    state.shuffleCursor = 0;
    state.phase = count == 0 ? NpcPatrolPhase::Failed : NpcPatrolPhase::Moving;
    state.resumePhase = state.phase;
    state.requestId = 0;
    state.destinationInitialized = false;
    state.waypointBaseYawRadians = 0.0f;
    state.lookOffsetRadians = 0.0f;
    state.lookDirection = 1.0f;
    state.retryRemainingSeconds = 0.0f;
    state.waitRemainingSeconds = 0.0f;
    state.slotIndex = -1;
    if (count == 0) {
        state.waypointIndex = 0;
        return;
    }
    if (randomStart) {
        state.waypointIndex = static_cast<size_t>(
                NextPatrolRandom(state.randomState)) % count;
    } else {
        state.waypointIndex = state.direction < 0 ? count - 1 : 0;
    }
    if (patrol.shuffleWaypoints) BuildShuffleOrder(state, count);
}

void AdvanceNpcPatrolWaypoint(
        NpcPatrolState& state,
        const SectorCompiledPatrol& patrol)
{
    const size_t count = patrol.waypoints.size();
    state.requestId = 0;
    state.destinationInitialized = false;
    state.waypointBaseYawRadians = 0.0f;
    state.lookOffsetRadians = 0.0f;
    state.lookDirection = 1.0f;
    if (count == 0) {
        state.phase = NpcPatrolPhase::Failed;
        return;
    }
    if (patrol.shuffleWaypoints) {
        if (state.shuffleCursor >= state.shuffleOrder.size()) {
            if (patrol.mode == SectorPatrolMode::Once || count <= 1) {
                state.phase = NpcPatrolPhase::Complete;
                return;
            }
            BuildShuffleOrder(state, count);
        }
        if (state.shuffleCursor >= state.shuffleOrder.size()) {
            state.phase = NpcPatrolPhase::Complete;
            return;
        }
        state.waypointIndex = state.shuffleOrder[state.shuffleCursor++];
        state.phase = NpcPatrolPhase::Moving;
        return;
    }
    if (patrol.mode == SectorPatrolMode::Once) {
        const bool atEnd = state.direction < 0
                ? state.waypointIndex == 0
                : state.waypointIndex + 1 >= count;
        if (atEnd) {
            state.phase = NpcPatrolPhase::Complete;
            return;
        }
        if (state.direction < 0) {
            --state.waypointIndex;
        } else {
            ++state.waypointIndex;
        }
    } else if (patrol.mode == SectorPatrolMode::Loop) {
        if (state.direction < 0) {
            state.waypointIndex = state.waypointIndex == 0
                    ? count - 1 : state.waypointIndex - 1;
        } else {
            state.waypointIndex = (state.waypointIndex + 1) % count;
        }
    } else if (count > 1) {
        if (state.direction > 0 && state.waypointIndex + 1 >= count) {
            state.direction = -1;
        } else if (state.direction < 0 && state.waypointIndex == 0) {
            state.direction = 1;
        }
        if (state.direction < 0) {
            --state.waypointIndex;
        } else {
            ++state.waypointIndex;
        }
    }
    state.phase = NpcPatrolPhase::Moving;
}

bool IsValidNpcInstanceId(std::string_view id)
{
    if (id.empty() || id.size() > 63) return false;
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
    });
}

const char* NpcMovePhaseName(NpcMovePhase phase)
{
    switch (phase) {
        case NpcMovePhase::Idle: return "idle";
        case NpcMovePhase::FollowingPath: return "following path";
        case NpcMovePhase::Arrived: return "arrived";
        case NpcMovePhase::Cancelled: return "cancelled";
        case NpcMovePhase::Failed: return "failed";
    }
    return "unknown";
}

const char* NpcMoveGaitName(NpcMoveGait gait)
{
    return gait == NpcMoveGait::Run ? "run" : "walk";
}

const char* NpcMoveAuthorityName(NpcMoveAuthority authority)
{
    switch (authority) {
        case NpcMoveAuthority::None: return "none";
        case NpcMoveAuthority::Programmatic: return "programmatic";
        case NpcMoveAuthority::Script: return "script";
        case NpcMoveAuthority::Patrol: return "patrol";
        case NpcMoveAuthority::Ai: return "AI";
    }
    return "unknown";
}

const char* NpcDoorTraversalPhaseName(NpcDoorTraversalPhase phase)
{
    switch (phase) {
        case NpcDoorTraversalPhase::None: return "None";
        case NpcDoorTraversalPhase::Approaching: return "Approaching";
        case NpcDoorTraversalPhase::WaitingForClearance: return "Waiting for clearance";
        case NpcDoorTraversalPhase::Crossing: return "Crossing";
    }
    return "Unknown";
}

const char* NpcPursuitSlotKindName(NpcPursuitSlotKind kind)
{
    switch (kind) {
        case NpcPursuitSlotKind::None: return "none";
        case NpcPursuitSlotKind::Melee: return "melee";
        case NpcPursuitSlotKind::Orbit: return "orbit";
        case NpcPursuitSlotKind::Invalid: return "invalid";
    }
    return "unknown";
}

const char* NpcVisualDetectionReasonName(NpcVisualDetectionReason reason)
{
    switch (reason) {
        case NpcVisualDetectionReason::NoPlayer: return "no player";
        case NpcVisualDetectionReason::OutsideRange: return "out of range";
        case NpcVisualDetectionReason::OutsideCone: return "outside cone";
        case NpcVisualDetectionReason::Occluded: return "occluded";
        case NpcVisualDetectionReason::Darkness: return "darkness";
        case NpcVisualDetectionReason::Building: return "building";
        case NpcVisualDetectionReason::Decaying: return "decaying";
        case NpcVisualDetectionReason::Detected: return "detected";
    }
    return "unknown";
}

} // namespace game
