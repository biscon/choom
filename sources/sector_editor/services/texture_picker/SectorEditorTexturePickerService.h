#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorTextureModals.h"

#include <functional>
#include <string>
#include <vector>

namespace game {

class SectorEditorTextureCatalogService;

struct SectorEditorSelectedTexture {
    bool valid = false;
    std::string textureId;
};

struct SectorEditorTexturePickerServiceCallbacks {
    std::function<void()> applySelection;
    std::function<std::string()> currentTextureForTarget;
};

void CloseSectorEditorTexturePicker(TexturePickerState& picker);
void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const std::vector<std::string>& textureIds,
        const std::string& currentTexture);
void OpenSectorEditorTexturePicker(
        TexturePickerState& picker,
        const std::vector<std::string>& textureIds,
        const std::string& currentTexture);
SectorEditorSelectedTexture CurrentSectorEditorTexturePickerSelection(const TexturePickerState& picker);

inline void DrawSectorEditorTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        TexturePickerState& picker,
        SectorEditorTextureCatalogService& textureCatalog,
        const SectorEditorTexturePickerServiceCallbacks& callbacks)
{
    const SectorEditorTexturePickerCallbacks modalCallbacks{
            [&picker]() { CloseSectorEditorTexturePicker(picker); },
            callbacks.applySelection,
            callbacks.currentTextureForTarget
    };
    DrawTexturePickerModal(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            picker,
            textureCatalog,
            modalCallbacks);
}

} // namespace game
