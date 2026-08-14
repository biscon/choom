#include "game/npc/NpcRuntime.h"

#include <algorithm>

namespace game {

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

} // namespace game
