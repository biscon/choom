#pragma once

#include "game/Health.h"
#include "game/npc/NpcRuntime.h"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace game {

inline constexpr size_t kNpcAiDebugLabelLineCount = 4;
inline constexpr size_t kNpcAiDebugLabelLineCapacity = 192;

struct NpcAiDebugVisionGeometry {
    Vector3 center{};
    Vector3 leftBoundary{};
    Vector3 rightBoundary{};
    float startRadians = 0.0f;
    float endRadians = 0.0f;
};

struct NpcAiDebugLabelData {
    std::array<std::array<char, kNpcAiDebugLabelLineCapacity>,
            kNpcAiDebugLabelLineCount> lines{};
    Color headingColor = WHITE;
};

NpcAiDebugVisionGeometry BuildNpcAiDebugVisionGeometry(
        Vector3 center,
        float yawRadians,
        float rangeWorld,
        float angleDegrees);

size_t NpcAiDebugRemainingCornerCount(
        const NpcNavigationRecord& navigation);

NpcAiDebugLabelData BuildNpcAiDebugLabelData(
        const NpcRuntimeInstance& npc,
        const NpcAiState& ai,
        const Health& health,
        const NpcNavigationRecord& navigation,
        Vector3 playerFeetPosition,
        bool aiFrozen,
        bool dead);

} // namespace game
