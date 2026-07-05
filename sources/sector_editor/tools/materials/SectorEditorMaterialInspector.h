#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorMaterialInspectorCallbacks {
    std::function<void(int, TopologyWallPart)> selectTopologySideDef;
    std::function<bool(int, bool)> setLineDefBlocksPlayer;
    std::function<void(int, TopologyWallPart, TopologyMaterialLayer)> openSideDefTexturePicker;
    std::function<bool(TopologySurfaceEditTarget)> copyTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager&)> pasteTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, float, engine::AssetManager*)> applyDecalOpacity;
    std::function<bool(TopologySurfaceEditTarget, bool, engine::AssetManager*)> applyDecalEmissive;
    std::function<bool(TopologySurfaceEditTarget, float, engine::AssetManager*)> applyDecalBloomIntensity;
    std::function<bool(TopologySurfaceEditTarget)> openDecalTintModal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> fitSelectedDecal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> clearSurfaceDecal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> clearMiddleTexture;
    std::function<bool(TopologySurfaceEditTarget, TopologyUvFitMode, engine::AssetManager*, TopologyMaterialLayer)> fitSelectedWallMaterial;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*, TopologyMaterialLayer)> alignSelectedWallMaterialVertical;
    std::function<bool(TopologySurfaceEditTarget, TopologyUAlignDirection, engine::AssetManager*, TopologyMaterialLayer)> alignSelectedWallMaterialU;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool(const char*, engine::AssetManager*)> finishTopologyMaterialMutation;
};

struct SectorEditorMaterialInspectorContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    engine::UIScrollAreaResult scroll;
    float contentW = 0.0f;
    float rowH = 0.0f;
    float gap = 0.0f;
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;
    const SectorEditorMaterialInspectorCallbacks& callbacks;
};

bool DrawTopologySideDefMaterialInspector(SectorEditorMaterialInspectorContext& context);

} // namespace game
