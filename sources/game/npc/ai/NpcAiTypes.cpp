#include "game/npc/ai/NpcAiTypes.h"

namespace game {

namespace {

constexpr float MeleeEngageToleranceWorld = 0.10f;
constexpr float MeleeDisengageToleranceWorld = 0.25f;

NpcAiIntent UpdateSeekAndDestroy(const NpcAiPluginInput& input)
{
    if (!input.playerAlive) return NpcAiIntent::Idle;
    const float tolerance = input.previousIntent == NpcAiIntent::AttackPlayer
            ? MeleeDisengageToleranceWorld
            : MeleeEngageToleranceWorld;
    if (input.attackCommitted
            || input.playerDistanceWorld
                    <= input.attackRangeWorld + tolerance) {
        return NpcAiIntent::AttackPlayer;
    }
    return NpcAiIntent::ChasePlayer;
}

} // namespace

const std::array<NpcAiTypeDescriptor, 1>& NpcAiTypeRegistry()
{
    static constexpr std::array<NpcAiTypeDescriptor, 1> types{{
            {kSeekAndDestroyNpcAiType, "Seek & Destroy",
                    NpcAiAlignment::Hostile, true, UpdateSeekAndDestroy}
    }};
    return types;
}

const NpcAiTypeDescriptor* FindNpcAiType(std::string_view id)
{
    for (const NpcAiTypeDescriptor& type : NpcAiTypeRegistry()) {
        if (id == type.id) return &type;
    }
    return nullptr;
}

bool IsNpcAiTypeCompatible(const NpcAiTypeDescriptor& type, bool hostile)
{
    return hostile
            ? type.alignment == NpcAiAlignment::Hostile
            : type.alignment == NpcAiAlignment::Friendly;
}

} // namespace game
