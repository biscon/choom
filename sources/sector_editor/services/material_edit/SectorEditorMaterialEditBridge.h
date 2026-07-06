#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

namespace game {

using SectorEditorMaterialEditActionFn =
        std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>;

struct SectorEditorMaterialEditBridgeCallbacks {
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool(const char*, engine::AssetManager*)> finishTopologyMaterialMutation;
    std::function<bool(const SectorEditorMaterialActionResult&, engine::AssetManager*)> finishMaterialActionResult;
    std::function<bool(
            TopologySurfaceEditTarget,
            engine::AssetManager*,
            SectorEditorMaterialEditActionFn)> applyAuthoringSideMaterialAction;
    std::function<bool(
            TopologySurfaceEditTarget,
            engine::AssetManager*,
            SectorEditorMaterialEditActionFn)> applyAuthoringFaceAnchorFlatMaterialAction;
};

struct SectorEditorMaterialEditBridgeContext {
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;
    const SectorEditorMaterialEditBridgeCallbacks& callbacks;
};

bool CopySectorEditorMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target);
bool PasteSectorEditorMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager& assets);
bool ApplySectorEditorSurfaceDecalOpacity(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        float opacity,
        engine::AssetManager* assets);
bool ApplySectorEditorSurfaceDecalEmissive(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets);
bool ApplySectorEditorSurfaceDecalTint(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets);
bool ApplySectorEditorSurfaceDecalBloomIntensity(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets);
bool OpenSectorEditorDecalTintModal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target);
bool ClearSectorEditorSurfaceDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets);
bool ClearSectorEditorMiddleTexture(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets);
bool FitSectorEditorSelectedDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets);
bool FitSectorEditorSelectedFlatDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets);
bool FitSectorEditorSelectedWallMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer);
bool AlignSectorEditorSelectedWallMaterialVertical(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer);
bool AlignSectorEditorSelectedWallMaterialU(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer);

} // namespace game
