#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/materials/SectorEditorMaterialRegistryEditorService.h"

namespace game {

enum class SectorEditorMaterialRegistryEditorResult {
    None,
    Saved,
    Cancelled
};

SectorEditorMaterialRegistryEditorResult DrawSectorEditorMaterialRegistryEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorMaterialRegistryEditorService& editor);

} // namespace game
