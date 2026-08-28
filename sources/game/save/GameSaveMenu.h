#pragma once

#include "engine/ui/UI.h"
#include "game/save/GameSaveData.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace game {

enum class GameSaveMenuMode {
    Closed,
    Save,
    Load
};

enum class GameSaveMenuActionType {
    None,
    Back,
    Save,
    Load
};

struct GameSaveMenuAction {
    GameSaveMenuActionType type = GameSaveMenuActionType::None;
    int slot = 0;
    std::string name;
};

struct GameSaveMenuState {
    GameSaveMenuMode mode = GameSaveMenuMode::Closed;
    std::vector<GameSaveSlotInfo> slots;
    std::array<engine::TextureHandle, GameSaveSlotCount> thumbnails{};
    engine::AssetScopeHandle thumbnailScope = engine::NullAssetScopeHandle();
    std::array<char, 256> nameBuffer{};
    int selectedSlot = 0;
    int firstVisibleSlot = 0;
    bool confirmationOpen = false;
    bool loadConfirmationRequired = false;
    std::string status;
};

void OpenGameSaveMenu(
        GameSaveMenuState& state,
        GameSaveMenuMode mode,
        bool loadConfirmationRequired,
        const std::filesystem::path& saveRoot,
        engine::AssetManager& assets);

void RefreshGameSaveMenu(
        GameSaveMenuState& state,
        const std::filesystem::path& saveRoot,
        engine::AssetManager& assets);

void CloseGameSaveMenu(
        GameSaveMenuState& state,
        engine::AssetManager& assets);

GameSaveMenuAction DrawGameSaveMenu(
        GameSaveMenuState& state,
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont);

} // namespace game
