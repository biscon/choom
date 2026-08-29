#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

namespace game {

constexpr engine::UIMenuShortcut SectorEditorAdjustSelectedShortcut()
{
    return engine::UIMenuShortcut{KEY_A, true};
}

constexpr bool CanBeginSectorEditorPreviewAdjustment(
        SectorEditorMode mode,
        bool hasAdjustableSelection,
        bool adjustmentActive)
{
    return mode == SectorEditorMode::Preview3D
            && hasAdjustableSelection
            && !adjustmentActive;
}

enum class SectorEditorMainMenuCommand : uint32_t {
    None = 0,
    NewLevel,
    LoadLevel,
    SaveLevel,
    SaveLevelAs,
    ReloadLevel,
    ClearGameSession,
    CopyConfig,
    PasteConfig,
    BeginPreviewAdjustment,
    ApplyPreviewAdjustment,
    CancelPreviewAdjustment,
    Toggle3DMode,
    OpenMaterialEditor,
    OpenSoundEditor,
    OpenPatrolEditor,
    OpenNpcEditor,
    OpenWeaponEditor,
    OpenItemEditor,
    ToggleShowGrid,
    ToggleShowAxes,
    ToggleShowIds,
    OpenLevelSettings,
    OpenColorSettings,
    OpenPlayerSettings,
    OpenSneakSettings
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
        bool gameSessionExists,
        bool canCopyConfig,
        bool canPasteConfig,
        bool hasAdjustablePreviewSelection,
        bool previewAdjustmentActive,
        bool visible,
        bool enabled);

} // namespace game
