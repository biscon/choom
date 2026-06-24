#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorPreviewSettingsModalCallbacks {
    std::function<void()> close;
    std::function<void()> apply;
    std::function<void()> openSkyTexturePicker;
};

void DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorPreviewSettingsModalState& modalState,
        bool texturePickerOpen,
        const SectorEditorPreviewSettingsModalCallbacks& callbacks);

} // namespace game
