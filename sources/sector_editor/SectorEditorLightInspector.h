#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorLightInspectorCallbacks {
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool()> deleteSelectedLight;
    std::function<bool()> bakeLightmaps;
};

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError);

bool DrawSelectedStaticLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyStaticPointLight& light,
        SectorEditorUiState& uiState,
        const SectorEditorLightInspectorCallbacks& callbacks);

} // namespace game
