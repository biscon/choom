#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorSectorInspectorCallbacks {
    std::function<bool()> tryRenameSelectedTopologySector;
    std::function<void()> openDeleteSelectedTopologySectorConfirmation;
    std::function<void()> startInsertSectorInside;
    std::function<void()> startPendingTopologySectorCut;
    std::function<void(const char*)> setStatusText;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool(float, float)> applySectorHeights;
    std::function<bool(bool)> applySectorCeilingSky;
    std::function<bool(float)> applySectorAmbientIntensity;
    std::function<bool(Color)> applySectorAmbientColor;
    std::function<bool(TopologySectorTextureField, const SectorTopologyUvSettings&)> applySectorUv;
    std::function<void(int, TopologySectorTextureField, TopologyMaterialLayer)> openTopologyTexturePicker;
    std::function<bool(TopologySurfaceEditTarget)> copyTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager&)> pasteTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, float)> applySurfaceDecalOpacity;
    std::function<bool(TopologySurfaceEditTarget, bool)> applySurfaceDecalEmissive;
    std::function<bool(TopologySurfaceEditTarget, float)> applySurfaceDecalBloomIntensity;
    std::function<bool(TopologySurfaceEditTarget)> openDecalTintModal;
    std::function<bool(TopologySurfaceEditTarget)> fitSelectedDecal;
    std::function<bool(TopologySurfaceEditTarget)> clearSurfaceDecal;
};

float SectorInspectorContentHeight(float rowH, float gap, bool hasIdError);

bool DrawTopologySectorInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologySector& sector,
        SectorEditorState& state,
        SectorEditorUiState& uiState,
        const SectorEditorSectorInspectorCallbacks& callbacks);

} // namespace game
