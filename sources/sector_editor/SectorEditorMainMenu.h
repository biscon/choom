#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

namespace game {

enum class SectorEditorMainMenuCommand : uint32_t {
    None = 0,
    NewLevel,
    LoadLevel,
    SaveLevel,
    SaveLevelAs,
    ReloadLevel,
    CopyConfig,
    PasteConfig,
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
        engine::FontHandle smallFont,
        const SectorEditorState& editorState,
        engine::UIMainMenuState& menuState,
        bool canCopyConfig,
        bool canPasteConfig,
        bool visible,
        bool enabled);

} // namespace game
