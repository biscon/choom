#pragma once

#include "game/save/GameSaveData.h"

#include <string>

namespace game {

bool SerializeGameSave(
        const GameSaveData& save,
        std::string& output,
        std::string& error);

bool DeserializeGameSave(
        const std::string& input,
        GameSaveData& save,
        std::string& error);

} // namespace game
