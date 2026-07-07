#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorVertexInspectorCallbacks {
    std::function<void()> clearStaleTopologySelection;
};

float SelectedVertexInspectorContentHeight();
float InspectedVertexInspectorContentHeight();

bool DrawTopologyVertexInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentW,
        float rowH,
        float gap,
        const SectorTopologyVertex* inspectedVertex,
        bool hasSelectedTopologyVertex,
        const SectorTopologyMap& topologyMap,
        SelectionState& selectionState,
        const SectorEditorVertexInspectorCallbacks& callbacks);

} // namespace game
