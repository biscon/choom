#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"

#include <functional>

namespace game {

struct SectorEditorSaveLevelModalCallbacks {
    std::function<void()> close;
    std::function<void()> save;
};

struct SectorEditorLoadLevelModalCallbacks {
    std::function<void()> close;
    std::function<void()> loadSelected;
};

struct SectorEditorConfirmationModalCallbacks {
    std::function<void()> cancel;
    std::function<void()> okay;
};

void DrawSectorEditorSaveLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SaveLevelModalState& modalState,
        const SectorEditorSaveLevelModalCallbacks& callbacks);

void DrawSectorEditorLoadLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        LoadLevelModalState& modalState,
        const SectorEditorLoadLevelModalCallbacks& callbacks);

void DrawSectorEditorConfirmationModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        ConfirmationModalState& modalState,
        const SectorEditorConfirmationModalCallbacks& callbacks);

} // namespace game
