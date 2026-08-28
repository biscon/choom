#pragma once

#include "engine/ui/UI.h"
#include "game/ApplicationFlow.h"
#include "game/FpsWeaponRegistry.h"

#include <optional>

namespace game {

std::optional<MainMenuAction> DrawGameMainMenu(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        bool gameRunning,
        const char* statusText);

bool DrawGameOverOverlay(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont);

enum class GameGraphicsSettingsAction {
    None,
    Apply,
    Cancel,
    Defaults
};

GameGraphicsSettingsAction DrawGameGraphicsSettings(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        FpsApplicationSettings& draft,
        const char* statusText);

} // namespace game
