#include "game/npc/ai/NpcAiDebugData.h"

#include "game/npc/ai/NpcAiTypes.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

const char* AwarenessName(NpcAwarenessState awareness)
{
    switch (awareness) {
        case NpcAwarenessState::Unaware: return "Unaware";
        case NpcAwarenessState::InvestigatingTravel:
            return "Investigate travel";
        case NpcAwarenessState::InvestigatingSearch:
            return "Investigate search";
        case NpcAwarenessState::Detected: return "Detected";
    }
    return "Unknown";
}

const char* IntentName(NpcAiIntent intent)
{
    switch (intent) {
        case NpcAiIntent::Idle: return "Idle";
        case NpcAiIntent::ChasePlayer: return "Chase";
        case NpcAiIntent::AttackPlayer: return "Attack";
    }
    return "Unknown";
}

Color AwarenessColor(NpcAwarenessState awareness, bool dead)
{
    if (dead) return Color{146, 152, 164, 255};
    switch (awareness) {
        case NpcAwarenessState::Unaware:
            return Color{184, 192, 204, 255};
        case NpcAwarenessState::InvestigatingTravel:
        case NpcAwarenessState::InvestigatingSearch:
            return Color{255, 208, 82, 255};
        case NpcAwarenessState::Detected:
            return Color{255, 92, 92, 255};
    }
    return WHITE;
}

template <size_t Capacity, typename... Args>
void Format(std::array<char, Capacity>& destination, const char* format,
        Args... args)
{
    std::snprintf(destination.data(), destination.size(), format, args...);
    destination.back() = '\0';
}

} // namespace

NpcAiDebugVisionGeometry BuildNpcAiDebugVisionGeometry(
        Vector3 center,
        float yawRadians,
        float rangeWorld,
        float angleDegrees)
{
    const float range = std::max(0.0f,
            std::isfinite(rangeWorld) ? rangeWorld : 0.0f);
    const float halfAngle = std::clamp(
            std::isfinite(angleDegrees) ? angleDegrees : 0.0f,
            0.0f,
            360.0f) * 0.5f * DEG2RAD;
    NpcAiDebugVisionGeometry result;
    result.center = center;
    result.startRadians = yawRadians - halfAngle;
    result.endRadians = yawRadians + halfAngle;
    result.leftBoundary = Vector3{
            center.x + std::sin(result.startRadians) * range,
            center.y,
            center.z + std::cos(result.startRadians) * range};
    result.rightBoundary = Vector3{
            center.x + std::sin(result.endRadians) * range,
            center.y,
            center.z + std::cos(result.endRadians) * range};
    return result;
}

size_t NpcAiDebugRemainingCornerCount(
        const NpcNavigationRecord& navigation)
{
    return navigation.cornerCount > navigation.nextCorner
            ? navigation.cornerCount - navigation.nextCorner : 0;
}

NpcAiDebugLabelData BuildNpcAiDebugLabelData(
        const NpcRuntimeInstance& npc,
        const NpcAiState& ai,
        const Health& health,
        const NpcNavigationRecord& navigation,
        Vector3 playerFeetPosition,
        bool aiFrozen,
        bool dead)
{
    NpcAiDebugLabelData result;
    result.headingColor = AwarenessColor(ai.awareness, dead);
    const NpcAiTypeDescriptor* descriptor = FindNpcAiType(ai.aiType);
    const char* aiName = descriptor != nullptr
            ? descriptor->displayName : ai.aiType.c_str();
    Format(result.lines[0], "%s | %s%s",
            npc.instanceId.c_str(),
            aiName != nullptr && aiName[0] != '\0' ? aiName : "No AI",
            aiFrozen && !dead ? " | FROZEN" : "");
    Format(result.lines[1], "%s | %s | %s | HP %d/%d",
            dead ? "Dead" : AwarenessName(ai.awareness),
            IntentName(ai.previousIntent),
            GetNpcActionMetadata(npc.action).displayName,
            health.current,
            health.maximum);
    const Vector2 playerDelta{
            playerFeetPosition.x - navigation.visualPosition.x,
            playerFeetPosition.z - navigation.visualPosition.z};
    Format(result.lines[2], "nav %s/%s/%s | %zu corners | player %.2fm",
            NpcMoveAuthorityName(navigation.authority),
            NpcMovePhaseName(navigation.phase),
            NpcMoveGaitName(navigation.gait),
            NpcAiDebugRemainingCornerCount(navigation),
            Vector2Length(playerDelta));
    if (dead) {
        Format(result.lines[3], "AI inactive | path request %llu",
                static_cast<unsigned long long>(navigation.requestId));
    } else if (ai.attackCommitted) {
        Format(result.lines[3], "attack committed | hit %s | range %.2fm",
                ai.attackHitResolved ? "resolved" : "pending",
                ai.attack.rangeWorld);
    } else if (ai.awareness == NpcAwarenessState::InvestigatingTravel
            || ai.awareness == NpcAwarenessState::InvestigatingSearch) {
        Format(result.lines[3], "last known %.2f, %.2f | search %.2fs",
                ai.lastKnownPlayerPosition.x,
                ai.lastKnownPlayerPosition.z,
                std::max(0.0f, ai.searchRemainingSeconds));
    } else {
        Format(result.lines[3], "vision %.1fm/%.0fdeg | hearing %.1fm | melee %.2fm",
                ai.perception.visionRangeWorld,
                ai.perception.visionAngleDegrees,
                ai.perception.hearingRangeWorld,
                ai.attack.rangeWorld);
    }
    if (ai.pursuitSlotIndex >= 0) {
        Format(result.lines[4], "slot %d | ring %d | %s%s",
                ai.pursuitSlotIndex,
                ai.pursuitSlotRing,
                NpcPursuitSlotKindName(ai.pursuitSlotKind),
                ai.pursuitRetargetFailed ? " | RETARGET FAILED" : " | claimed");
    } else {
        Format(result.lines[4], "slot none");
    }
    return result;
}

} // namespace game
