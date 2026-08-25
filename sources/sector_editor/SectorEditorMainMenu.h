#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

namespace game {

enum class SectorEditorMainMenuCommand : uint32_t {
    None = 0,
    NewLevel,
    LoadLevel,
    SaveLevel,
    ReloadLevel,
    Toggle3DMode,
    OpenMaterialEditor,
    OpenSoundEditor,
    OpenNpcEditor,
    OpenWeaponEditor,
    ToggleShowGrid,
    ToggleShowAxes,
    ToggleShowIds,
    OpenLevelSettings
};

SectorEditorMainMenuCommand DrawSectorEditorMainMenu(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const SectorEditorState& editorState,
        engine::UIMainMenuState& menuState,
        bool enabled);

} // namespace game
