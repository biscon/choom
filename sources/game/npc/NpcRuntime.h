#pragma once

#include "game/npc/NpcDefinitions.h"

#include <string>
#include <string_view>

namespace game {

struct NpcRuntimeInstance {
    std::string definitionId;
    std::string instanceId;
    NpcAction action = NpcAction::Idle;
    bool hostile = false;
    float walkSpeed = 1.5f;
    float runSpeed = 3.0f;
};

bool IsValidNpcInstanceId(std::string_view id);

} // namespace game
