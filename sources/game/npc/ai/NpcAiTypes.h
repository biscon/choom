#pragma once

#include <array>
#include <string_view>

namespace game {

enum class NpcAiAlignment {
    Friendly,
    Hostile
};

enum class NpcAiIntent {
    Idle,
    ChasePlayer,
    AttackPlayer
};

struct NpcAiPluginInput {
    float playerDistanceWorld = 0.0f;
    float attackRangeWorld = 0.0f;
    bool attackCommitted = false;
    bool playerAlive = true;
    NpcAiIntent previousIntent = NpcAiIntent::Idle;
};

using NpcAiUpdateFn = NpcAiIntent (*)(const NpcAiPluginInput& input);

struct NpcAiTypeDescriptor {
    const char* id = "";
    const char* displayName = "";
    NpcAiAlignment alignment = NpcAiAlignment::Friendly;
    bool usesPlayerAwareness = false;
    NpcAiUpdateFn update = nullptr;
};

inline constexpr const char* kSeekAndDestroyNpcAiType = "seek_and_destroy";

const std::array<NpcAiTypeDescriptor, 1>& NpcAiTypeRegistry();
const NpcAiTypeDescriptor* FindNpcAiType(std::string_view id);
bool IsNpcAiTypeCompatible(const NpcAiTypeDescriptor& type, bool hostile);

} // namespace game
