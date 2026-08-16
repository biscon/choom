#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"

namespace game {

enum class SectorEditorModelPickerModalResult {
    None,
    Cancelled,
    Selected
};

SectorEditorModelPickerModalResult DrawSectorEditorModelPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorStaticModelPickerService& picker);

} // namespace game
