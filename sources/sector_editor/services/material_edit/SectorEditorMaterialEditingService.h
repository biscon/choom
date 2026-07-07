#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorAuthoringMaterialTarget {
    TopologySurfaceEditTarget target;
    TopologyMaterialLayer layer = TopologyMaterialLayer::Base;
    std::string* textureId = nullptr;
    SectorTopologyUvSettings* uv = nullptr;
    SectorTopologyDecalLayer* decal = nullptr;
    SectorTopologyWallPartSettings* wallPart = nullptr;
};

using SectorEditorAuthoringMaterialActionFn =
        std::function<SectorEditorMaterialActionResult(SectorEditorAuthoringMaterialTarget&)>;

struct SectorEditorMaterialEditingServiceContext {
    SectorEditorState& state;
    MaterialEditingState& materialState;
    MaterialEditingUiState& materialUiState;
    TexturePickerState& texturePicker;
    std::string& statusText;
    std::function<bool(engine::AssetManager*)> requestPreviewMaterialMeshRebuild;
};

class SectorEditorMaterialEditingService {
public:
    explicit SectorEditorMaterialEditingService(SectorEditorMaterialEditingServiceContext context);

    bool CopyMaterial(TopologySurfaceEditTarget target);
    bool PasteMaterial(TopologySurfaceEditTarget target, engine::AssetManager& assets);

    bool ApplySurfaceUvValue(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer,
            int component,
            float value,
            SectorSurfaceKind surfaceKind,
            engine::AssetManager& assets);
    bool ResetSurfaceUv(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer,
            SectorSurfaceKind surfaceKind,
            engine::AssetManager& assets);
    bool ApplyInspectorSideDefUvValue(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer,
            int component,
            float value,
            engine::AssetManager& assets);
    bool ResetInspectorSideDefUv(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer,
            engine::AssetManager& assets);

    bool ApplyDecalOpacity(TopologySurfaceEditTarget target, float opacity, engine::AssetManager* assets);
    bool ApplyDecalEmissive(TopologySurfaceEditTarget target, bool emissive, engine::AssetManager* assets);
    bool ApplyDecalTint(TopologySurfaceEditTarget target, Vector3 tint, engine::AssetManager* assets);
    bool ApplyDecalBloomIntensity(
            TopologySurfaceEditTarget target,
            float bloomIntensity,
            engine::AssetManager* assets);
    bool OpenDecalTintModal(TopologySurfaceEditTarget target);

    bool ClearSurfaceDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool ClearMiddleTexture(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedFlatDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedWallMaterial(
            TopologySurfaceEditTarget target,
            TopologyUvFitMode mode,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialVertical(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialU(
            TopologySurfaceEditTarget target,
            TopologyUAlignDirection direction,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer);

    const SectorTopologyDecalLayer* DecalForSurface(TopologySurfaceEditTarget target) const;
    const SectorTopologyUvSettings* UvForSurface(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer) const;
    bool IsDecalAssigned(TopologySurfaceEditTarget target) const;
    bool IsValidSurfaceTarget(TopologySurfaceEditTarget target) const;
    std::string CurrentTextureForSurface(TopologySurfaceEditTarget target, TopologyMaterialLayer layer) const;

    std::string CurrentTextureForPickerTarget() const;
    bool OpenMaterialPickerForDerivedSector(
            int topologySectorId,
            TopologySectorTextureField field,
            TopologyMaterialLayer layer);
    bool OpenMaterialPickerForAuthoringFaceAnchor(
            int faceAnchorId,
            TopologySectorTextureField field,
            TopologyMaterialLayer layer);
    bool OpenMaterialPickerForDerivedSideDef(
            int topologySideDefId,
            TopologyWallPart wallPart,
            TopologyMaterialLayer layer);
    bool OpenMaterialPickerForAuthoringSide(
            SectorAuthoringSideId sideId,
            TopologyWallPart wallPart,
            TopologyMaterialLayer layer);
    SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(engine::AssetManager* assets);

private:
    bool ApplyMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer,
            const SectorEditorAuthoringMaterialActionFn& action);
    bool ApplyAuthoringSideMaterialEdit(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer,
            const SectorEditorAuthoringMaterialActionFn& action);
    bool ApplyAuthoringFaceAnchorMaterialEdit(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            TopologyMaterialLayer layer,
            const SectorEditorAuthoringMaterialActionFn& action);
    bool FinishAuthoringMaterialActionResult(
            const SectorEditorMaterialActionResult& result,
            bool refreshed,
            engine::AssetManager* assets,
            const char* failureStatus);
    void MarkTopologyDocumentEdited(const char* status);
    void ApplyMaterialUiResetFlags(const SectorEditorMaterialActionResult& result);

    SectorEditorMaterialEditingServiceContext context_;
};

} // namespace game
