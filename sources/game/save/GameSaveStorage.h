#pragma once

#include "game/save/GameSaveData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace game {

std::filesystem::path ResolveGameSaveRoot(
        const std::string& applicationId = "engine");
std::filesystem::path GameSaveSlotPath(
        const std::filesystem::path& root,
        int slot);

bool IsValidGameSaveName(const std::string& name);
std::string CurrentGameSaveTimestampUtc();
std::string FormatGameSaveTimestampLocal(const std::string& utcTimestamp);

std::vector<GameSaveSlotInfo> ScanGameSaveSlots(
        const std::filesystem::path& root);

bool LoadGameSaveSlot(
        const std::filesystem::path& root,
        int slot,
        GameSaveData& save,
        std::string& error);

bool WriteGameSaveSlot(
        const std::filesystem::path& root,
        const GameSaveData& save,
        std::string& error);

} // namespace game
