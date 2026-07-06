#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

namespace game {

using SectorEditorMaterialEditingActionFn =
        std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>;

struct SectorEditorMaterialEditingServiceContext {
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    TexturePickerState& texturePicker;
    std::string& statusText;
    std::function<bool(engine::AssetManager*)> requestPreviewMaterialMeshRebuild;
};

class SectorEditorMaterialEditingService {
public:
    explicit SectorEditorMaterialEditingService(SectorEditorMaterialEditingServiceContext context);

    bool FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets);
    bool FinishMaterialActionResult(const SectorEditorMaterialActionResult& result, engine::AssetManager* assets);

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
    SectorTopologyDecalLayer* MutableDecalForSurface(TopologySurfaceEditTarget target);
    const SectorTopologyUvSettings* UvForSurface(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer) const;
    SectorTopologyUvSettings* MutableUvForSurface(
            TopologySurfaceEditTarget target,
            TopologyMaterialLayer layer);
    bool IsDecalAssigned(TopologySurfaceEditTarget target) const;
    bool IsValidSurfaceTarget(TopologySurfaceEditTarget target) const;
    std::string CurrentTextureForSurface(TopologySurfaceEditTarget target, TopologyMaterialLayer layer) const;

    std::string CurrentTextureForPickerTarget() const;
    bool OpenTexturePickerForSector(
            int sectorId,
            TopologySectorTextureField field,
            TopologyMaterialLayer layer);
    bool OpenTexturePickerForSideDef(
            int sideDefId,
            TopologyWallPart wallPart,
            TopologyMaterialLayer layer);
    bool OpenTexturePickerForAuthoringFaceAnchor(
            int topologySectorId,
            TopologySectorTextureField field,
            TopologyMaterialLayer layer);
    bool OpenTexturePickerForAuthoringFaceAnchorById(
            int faceAnchorId,
            TopologySectorTextureField field,
            TopologyMaterialLayer layer);
    bool OpenTexturePickerForAuthoringSide(
            int topologySideDefId,
            TopologyWallPart wallPart,
            TopologyMaterialLayer layer);
    bool OpenTexturePickerForAuthoringSideById(
            SectorAuthoringSideId sideId,
            TopologyWallPart wallPart,
            TopologyMaterialLayer layer);
    SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(engine::AssetManager* assets);

private:
    bool ApplyMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            const SectorEditorMaterialEditingActionFn& action);
    bool ApplyAuthoringSideMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            const SectorEditorMaterialEditingActionFn& action);
    bool ApplyAuthoringFaceAnchorFlatMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            const SectorEditorMaterialEditingActionFn& action);
    bool FinishAuthoringSideMaterialActionResult(
            TopologySurfaceEditTarget target,
            const SectorEditorMaterialActionResult& result,
            const SectorTopologyMap& editedTopology,
            engine::AssetManager* assets);
    void MarkTopologyDocumentEdited(const char* status);
    void ApplyMaterialUiResetFlags(const SectorEditorMaterialActionResult& result);

    SectorEditorMaterialEditingServiceContext context_;
};

} // namespace game
