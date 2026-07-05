#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorTextureModals.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorSelectedTexture {
    bool valid = false;
    std::string textureId;
};

struct SectorEditorTexturePickerServiceCallbacks {
    std::function<void()> applySelection;
    std::function<std::string()> currentTextureForTarget;
    std::function<engine::TextureHandle(const std::string&)> textureHandleForId;
};

void CloseSectorEditorTexturePicker(TexturePickerState& picker);
void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const std::string& currentTexture);
void OpenSectorEditorTexturePicker(
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const std::string& currentTexture);
SectorEditorSelectedTexture CurrentSectorEditorTexturePickerSelection(const TexturePickerState& picker);

inline void DrawSectorEditorTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const SectorEditorTexturePickerServiceCallbacks& callbacks)
{
    const SectorEditorTexturePickerCallbacks modalCallbacks{
            [&picker]() { CloseSectorEditorTexturePicker(picker); },
            callbacks.applySelection,
            callbacks.currentTextureForTarget,
            callbacks.textureHandleForId
    };
    DrawTexturePickerModal(ui, config, input, assets, font, picker, map, modalCallbacks);
}

} // namespace game
