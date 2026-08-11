#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_demo/SectorAuthoringGraph.h"

#include <cstddef>
#include <functional>

namespace game {

inline void OpenSectorEditorSetAllModal(
        SectorEditorSetAllModalState& modalState,
        const SectorAuthoringGraph& authoringGraph,
        SectorAuthoringSelectionTarget selectedAuthoring)
{
    const SectorAuthoringFaceAnchor* source = nullptr;
    if (selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor) {
        for (const SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
            if (anchor.id == selectedAuthoring.faceAnchorId && !anchor.isVoid) {
                source = &anchor;
                break;
            }
        }
    }
    if (source == nullptr) {
        for (const SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
            if (!anchor.isVoid) {
                source = &anchor;
                break;
            }
        }
    }

    modalState = SectorEditorSetAllModalState{};
    modalState.open = true;
    if (source != nullptr) {
        modalState.ambientIntensity = source->ambientIntensity;
        modalState.ambientColor = source->ambientColor;
        modalState.ambientColor.a = 255;
    }
}

struct SectorEditorSetAllModalCallbacks {
    std::function<void()> close;
    std::function<void(SectorEditorSectorLightingScope, float, Color)> applySectorLighting;
};

void DrawSectorEditorSetAllModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorSetAllModalState& modalState,
        std::size_t selectedSectorCount,
        const SectorEditorSetAllModalCallbacks& callbacks);

} // namespace game
