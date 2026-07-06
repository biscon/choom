#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

#include <cmath>
#include <utility>

namespace game {

namespace {

bool IsFlatTopologySurfaceTarget(TopologySurfaceEditTarget target)
{
    return target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

bool IsSelectedSurface3DFlatTarget(const SectorEditorState& state, TopologySurfaceEditTarget target)
{
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
        return state.selectedSurface3D.kind == SectorSurfaceKind::Floor
                && state.selectedSurface3D.topologySectorId == target.sectorId;
    }
    if (target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        return state.selectedSurface3D.kind == SectorSurfaceKind::Ceiling
                && state.selectedSurface3D.topologySectorId == target.sectorId;
    }
    return false;
}

SectorSurfaceRef FlatSurfaceForTarget(const SectorEditorState& state, TopologySurfaceEditTarget target)
{
    SectorSurfaceRef surface = state.selectedSurface3D;
    if (!IsSelectedSurface3DFlatTarget(state, target) && IsFlatTopologySurfaceTarget(target)) {
        surface = SectorSurfaceRef{};
        surface.kind = target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? SectorSurfaceKind::Floor
                : SectorSurfaceKind::Ceiling;
        surface.topologySectorId = target.sectorId;
    }
    return surface;
}

void ResetSurface3DUi(SectorEditorState& state, SectorEditorUiState& uiState)
{
    SectorEditorSelectionServiceContext context{state, uiState, nullptr, nullptr, nullptr};
    ResetSectorEditorSurface3DUiState(context);
}

void InvalidateTopologyRenderCache(SectorEditorState& state)
{
    ++state.topologyRenderRevision;
    state.topologyRenderCache.valid = false;
}

bool ValidateUvComponentValue(int component, float value)
{
    if (!std::isfinite(value)) {
        return false;
    }
    if ((component == 0 || component == 1)
            && (value < TopologyUvScaleMin || value > TopologyUvScaleMax)) {
        return false;
    }
    return component >= 0 && component <= 3;
}

void AssignUvComponent(SectorTopologyUvSettings& uv, int component, float value)
{
    switch (component) {
        case 0: uv.scale.x = value; break;
        case 1: uv.scale.y = value; break;
        case 2: uv.offset.x = value; break;
        case 3: uv.offset.y = value; break;
        default: break;
    }
}

bool HasNonDefaultUv(const SectorTopologyUvSettings& uv)
{
    return uv.scale.x != 1.0f
            || uv.scale.y != 1.0f
            || uv.offset.x != 0.0f
            || uv.offset.y != 0.0f;
}

SectorTopologyWallPartSettings& AuthoringWallPartSettingsFor(
        SectorAuthoringLineSide& side,
        TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return side.wall;
        case TopologyWallPart::Lower: return side.lower;
        case TopologyWallPart::Upper: return side.upper;
        case TopologyWallPart::Middle: return side.middle;
    }
    return side.wall;
}

} // namespace

SectorEditorMaterialEditingService::SectorEditorMaterialEditingService(
        SectorEditorMaterialEditingServiceContext context)
    : context_(std::move(context))
{
}

void SectorEditorMaterialEditingService::MarkTopologyDocumentEdited(const char* status)
{
    context_.state.topologyDocumentDirty = true;
    context_.state.hasUnsavedChanges = true;
    InvalidateTopologyRenderCache(context_.state);
    if (status != nullptr && status[0] != '\0') {
        context_.statusText = status;
    }
}

void SectorEditorMaterialEditingService::ApplyMaterialUiResetFlags(
        const SectorEditorMaterialActionResult& result)
{
    if (result.resetSurface3DUi) {
        ResetSurface3DUi(context_.state, context_.uiState);
    }
    if (result.resetSectorUvInputs) {
        for (engine::UIFloatInputState& inputState : context_.uiState.topologySectorUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetSideDefUvInputs) {
        for (engine::UIFloatInputState& inputState : context_.uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetDecalInputs) {
        for (engine::UIFloatInputState& inputState : context_.uiState.topologySectorDecalOpacityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        for (engine::UIFloatInputState& inputState : context_.uiState.topologySectorDecalBloomIntensityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        context_.uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        context_.uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        context_.uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
        context_.uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    if (result.closeDecalTintModal) {
        context_.state.decalTintModal = DecalTintModalState{};
    }
}

bool SectorEditorMaterialEditingService::FinishTopologyMaterialMutation(
        const char* status,
        engine::AssetManager* assets)
{
    context_.state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(status);
    if (assets != nullptr && context_.requestPreviewMaterialMeshRebuild) {
        return context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return true;
}

bool SectorEditorMaterialEditingService::FinishMaterialActionResult(
        const SectorEditorMaterialActionResult& result,
        engine::AssetManager* assets)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            context_.statusText = result.status;
        }
        return false;
    }

    ApplyMaterialUiResetFlags(result);
    return FinishTopologyMaterialMutation(result.status.c_str(), assets);
}

bool SectorEditorMaterialEditingService::FinishAuthoringSideMaterialActionResult(
        TopologySurfaceEditTarget target,
        const SectorEditorMaterialActionResult& result,
        const SectorTopologyMap& editedTopology,
        engine::AssetManager* assets)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            context_.statusText = result.status;
        }
        return false;
    }

    const SectorTopologySideDef* editedSideDef =
            FindSectorTopologySideDef(editedTopology, target.sideDefId);
    if (editedSideDef == nullptr) {
        context_.statusText = "Selected authoring side material target is no longer valid.";
        return false;
    }

    ApplyMaterialUiResetFlags(result);
    context_.state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.state,
            target.sideDefId,
            result.status.c_str(),
            [editedSideDef](SectorAuthoringLineSide& side) {
                side.wall = editedSideDef->wall;
                side.lower = editedSideDef->lower;
                side.upper = editedSideDef->upper;
                side.middle = editedSideDef->middle;
                return true;
            });
    if (!refreshed) {
        context_.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
    }
    if (assets != nullptr && refreshed && context_.requestPreviewMaterialMeshRebuild) {
        return context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return refreshed;
}

bool SectorEditorMaterialEditingService::ApplyAuthoringSideMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const SectorEditorMaterialEditingActionFn& action)
{
    if (!IsWallTopologyEditTarget(target.kind) || !HasAuthoringGraphData(context_.state)) {
        return false;
    }
    if (context_.state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || context_.state.authoringDerivedTopologyStale
            || !context_.state.authoringDerivation.success) {
        context_.statusText = "Wall material edit unavailable: derived topology is not current";
        return true;
    }
    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.state,
                target.sideDefId,
                sideId)) {
        context_.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
        return true;
    }
    if (!action) {
        return false;
    }

    SectorTopologyMap editedTopology = context_.state.topologyMap;
    return FinishAuthoringSideMaterialActionResult(
            target,
            action(editedTopology),
            editedTopology,
            assets);
}

bool SectorEditorMaterialEditingService::ApplyAuthoringFaceAnchorFlatMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const SectorEditorMaterialEditingActionFn& action)
{
    SectorEditorAuthoringFlatMaterialActionResult result;
    if (!game::ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction(
                context_.state,
                FlatSurfaceForTarget(context_.state, target),
                target,
                action,
                &result)) {
        return false;
    }

    ApplyMaterialUiResetFlags(result.materialResult);
    if (!result.status.empty()) {
        context_.statusText = result.status;
    }
    if (assets != nullptr && result.changed && context_.requestPreviewMaterialMeshRebuild) {
        context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return true;
}

bool SectorEditorMaterialEditingService::ApplyMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const SectorEditorMaterialEditingActionFn& action)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData(context_.state)) {
        return ApplyAuthoringSideMaterialAction(target, assets, action);
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData(context_.state)) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(target, assets, action);
    }
    return FinishMaterialActionResult(action(context_.state.topologyMap), assets);
}

bool SectorEditorMaterialEditingService::CopyMaterial(TopologySurfaceEditTarget target)
{
    TopologyMaterialPayload payload;
    std::string status;
    if (!CopyMaterialSurface(context_.state.topologyMap, target, payload, status)) {
        context_.statusText = status;
        return false;
    }
    context_.state.copiedTopologyMaterial = payload;
    context_.statusText = status;
    return true;
}

bool SectorEditorMaterialEditingService::PasteMaterial(
        TopologySurfaceEditTarget target,
        engine::AssetManager& assets)
{
    return ApplyMaterialAction(
            target,
            &assets,
            [this, target](SectorTopologyMap& map) {
                return PasteMaterialSurface(map, target, context_.state.copiedTopologyMaterial);
            });
}

bool SectorEditorMaterialEditingService::ApplySurfaceUvValue(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        int component,
        float value,
        SectorSurfaceKind surfaceKind,
        engine::AssetManager& assets)
{
    return ApplyMaterialAction(
            target,
            &assets,
            [target, layer, component, value, surfaceKind](SectorTopologyMap& map) {
                return game::ApplySurfaceUvValue(map, target, layer, surfaceKind, component, value);
            });
}

bool SectorEditorMaterialEditingService::ResetSurfaceUv(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        engine::AssetManager& assets)
{
    return ApplyMaterialAction(
            target,
            &assets,
            [target, layer, surfaceKind](SectorTopologyMap& map) {
                return game::ResetSurfaceUv(map, target, layer, surfaceKind);
            });
}

bool SectorEditorMaterialEditingService::ApplyInspectorSideDefUvValue(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        int component,
        float value,
        engine::AssetManager& assets)
{
    if (!ValidateUvComponentValue(component, value)) {
        context_.statusText = "Invalid topology sidedef UV value";
        return false;
    }
    if (!IsWallTopologyEditTarget(target.kind)) {
        context_.statusText = "Selected material target is no longer valid.";
        return false;
    }
    const TopologyMaterialLayer effectiveLayer = EffectiveTopologyMaterialLayer(target.kind, layer);
    const SectorTopologyUvSettings* derivedUv =
            UvForMaterialSurface(context_.state.topologyMap, target, effectiveLayer);
    if (derivedUv == nullptr) {
        context_.statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }
    const char* status = TextFormat(
            "Updated topology sidedef %d %s %s UV",
            target.sideDefId,
            TopologyWallPartStatusName(TopologyEditTargetWallPart(target.kind)),
            TopologyMaterialLayerStatusName(effectiveLayer));

    if (!HasAuthoringGraphData(context_.state)) {
        context_.statusText = "Cannot edit sidedef UV: material UV editor requires authoring data.";
        return false;
    }
    if (context_.state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || context_.state.authoringDerivedTopologyStale
            || !context_.state.authoringDerivation.success) {
        context_.statusText = "Cannot edit sidedef UV: derived topology is not current.";
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.state,
                target.sideDefId,
                sideId)) {
        context_.statusText = "Cannot edit sidedef UV: selected topology side is not mapped to an authoring line.";
        return false;
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    context_.state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.state,
            target.sideDefId,
            status,
            [wallPart, effectiveLayer, component, value](SectorAuthoringLineSide& side) {
                SectorTopologyWallPartSettings& part = AuthoringWallPartSettingsFor(side, wallPart);
                SectorTopologyUvSettings& authoringUv = effectiveLayer == TopologyMaterialLayer::Decal
                        ? part.decal.uv
                        : part.uv;
                AssignUvComponent(authoringUv, component, value);
                return true;
            });
    if (!refreshed) {
        context_.statusText = "Cannot edit sidedef UV: selected topology side is not mapped to an authoring line.";
        return false;
    }
    if (target.kind == TopologySurfaceEditTargetKind::SideDefMiddle
            && context_.requestPreviewMaterialMeshRebuild) {
        return context_.requestPreviewMaterialMeshRebuild(&assets);
    }
    return true;
}

bool SectorEditorMaterialEditingService::ResetInspectorSideDefUv(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        engine::AssetManager& assets)
{
    if (!IsWallTopologyEditTarget(target.kind)) {
        context_.statusText = "Selected material target is no longer valid.";
        return false;
    }
    const TopologyMaterialLayer effectiveLayer = EffectiveTopologyMaterialLayer(target.kind, layer);
    const SectorTopologyUvSettings* derivedUv =
            UvForMaterialSurface(context_.state.topologyMap, target, effectiveLayer);
    if (derivedUv == nullptr) {
        context_.statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }

    if (!HasAuthoringGraphData(context_.state)) {
        context_.statusText = "Cannot edit sidedef UV: material UV editor requires authoring data.";
        return false;
    }
    if (context_.state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || context_.state.authoringDerivedTopologyStale
            || !context_.state.authoringDerivation.success) {
        context_.statusText = "Cannot edit sidedef UV: derived topology is not current.";
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.state,
                target.sideDefId,
                sideId)) {
        context_.statusText = "Cannot edit sidedef UV: selected topology side is not mapped to an authoring line.";
        return false;
    }
    if (!HasNonDefaultUv(*derivedUv)) {
        return false;
    }

    const char* status = target.kind == TopologySurfaceEditTargetKind::SideDefMiddle
            ? "Reset middle UV."
            : TextFormat(
                    "Reset topology sidedef %d %s %s UV",
                    target.sideDefId,
                    TopologyWallPartStatusName(TopologyEditTargetWallPart(target.kind)),
                    TopologyMaterialLayerStatusName(effectiveLayer));

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    context_.state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.state,
            target.sideDefId,
            status,
            [wallPart, effectiveLayer](SectorAuthoringLineSide& side) {
                SectorTopologyWallPartSettings& part = AuthoringWallPartSettingsFor(side, wallPart);
                SectorTopologyUvSettings& authoringUv = effectiveLayer == TopologyMaterialLayer::Decal
                        ? part.decal.uv
                        : part.uv;
                ResetTopologyUv(authoringUv);
                return true;
            });
    if (!refreshed) {
        context_.statusText = "Cannot edit sidedef UV: selected topology side is not mapped to an authoring line.";
        return false;
    }
    for (engine::UIFloatInputState& inputState : context_.uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    if (target.kind == TopologySurfaceEditTargetKind::SideDefMiddle
            && context_.requestPreviewMaterialMeshRebuild) {
        return context_.requestPreviewMaterialMeshRebuild(&assets);
    }
    return true;
}

bool SectorEditorMaterialEditingService::ApplyDecalOpacity(
        TopologySurfaceEditTarget target,
        float opacity,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, opacity](SectorTopologyMap& map) {
                return game::ApplySurfaceDecalOpacity(map, target, opacity);
            });
}

bool SectorEditorMaterialEditingService::ApplyDecalEmissive(
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, emissive](SectorTopologyMap& map) {
                return game::ApplySurfaceDecalEmissive(map, target, emissive);
            });
}

bool SectorEditorMaterialEditingService::ApplyDecalTint(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, tint](SectorTopologyMap& map) {
                return game::ApplySurfaceDecalTint(map, target, tint);
            });
}

bool SectorEditorMaterialEditingService::ApplyDecalBloomIntensity(
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, bloomIntensity](SectorTopologyMap& map) {
                return game::ApplySurfaceDecalBloomIntensity(map, target, bloomIntensity);
            });
}

bool SectorEditorMaterialEditingService::OpenDecalTintModal(TopologySurfaceEditTarget target)
{
    DecalTintModalState modal;
    std::string status;
    if (!BuildDecalTintModal(context_.state.topologyMap, target, modal, status)) {
        context_.statusText = status;
        return false;
    }
    context_.state.decalTintModal = modal;
    return true;
}

bool SectorEditorMaterialEditingService::ClearSurfaceDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return game::ClearSurfaceDecal(map, target);
            });
}

bool SectorEditorMaterialEditingService::ClearMiddleTexture(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return game::ClearMiddleTexture(map, target);
            });
}

bool SectorEditorMaterialEditingService::FitSelectedDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return game::FitSelectedDecal(map, target);
            });
}

bool SectorEditorMaterialEditingService::FitSelectedFlatDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return game::FitSelectedFlatDecal(map, target);
            });
}

bool SectorEditorMaterialEditingService::FitSelectedWallMaterial(
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, mode, layer](SectorTopologyMap& map) {
                return game::FitSelectedWallMaterial(map, target, mode, layer);
            });
}

bool SectorEditorMaterialEditingService::AlignSelectedWallMaterialVertical(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, layer](SectorTopologyMap& map) {
                return game::AlignSelectedWallMaterialVertical(map, target, layer);
            });
}

bool SectorEditorMaterialEditingService::AlignSelectedWallMaterialU(
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    return ApplyMaterialAction(
            target,
            assets,
            [target, direction, layer](SectorTopologyMap& map) {
                return game::AlignSelectedWallMaterialU(map, target, direction, layer);
            });
}

const SectorTopologyDecalLayer* SectorEditorMaterialEditingService::DecalForSurface(
        TopologySurfaceEditTarget target) const
{
    return game::DecalForMaterialSurface(context_.state.topologyMap, target);
}

SectorTopologyDecalLayer* SectorEditorMaterialEditingService::MutableDecalForSurface(
        TopologySurfaceEditTarget target)
{
    return game::MutableDecalForMaterialSurface(context_.state.topologyMap, target);
}

const SectorTopologyUvSettings* SectorEditorMaterialEditingService::UvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::UvForMaterialSurface(context_.state.topologyMap, target, layer);
}

SectorTopologyUvSettings* SectorEditorMaterialEditingService::MutableUvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    return game::MutableUvForMaterialSurface(context_.state.topologyMap, target, layer);
}

bool SectorEditorMaterialEditingService::IsDecalAssigned(TopologySurfaceEditTarget target) const
{
    return game::IsMaterialDecalAssigned(context_.state.topologyMap, target);
}

bool SectorEditorMaterialEditingService::IsValidSurfaceTarget(TopologySurfaceEditTarget target) const
{
    return game::IsValidMaterialSurfaceTarget(context_.state.topologyMap, target);
}

std::string SectorEditorMaterialEditingService::CurrentTextureForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::CurrentTextureForMaterialSurface(context_.state.topologyMap, target, layer);
}

std::string SectorEditorMaterialEditingService::CurrentTextureForPickerTarget() const
{
    return CurrentSectorEditorMaterialPickerTexture(context_.state, context_.texturePicker);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForSector(
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForSector(context_.state, sectorId, field, layer);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForSideDef(
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForSideDef(context_.state, sideDefId, wallPart, layer);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForAuthoringFaceAnchor(
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
            context_.state,
            topologySectorId,
            field,
            layer);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForAuthoringFaceAnchorById(
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchorById(
            context_.state,
            faceAnchorId,
            field,
            layer);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForAuthoringSide(
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSide(
            context_.state,
            topologySideDefId,
            wallPart,
            layer);
}

bool SectorEditorMaterialEditingService::OpenTexturePickerForAuthoringSideById(
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSideById(
            context_.state,
            sideId,
            wallPart,
            layer);
}

SectorEditorTexturePickerApplyResult SectorEditorMaterialEditingService::ApplyTexturePickerSelection(
        engine::AssetManager* assets)
{
    if (!context_.texturePicker.open
            || !IsSectorEditorMaterialTexturePickerTarget(context_.texturePicker.topologyTargetKind)) {
        return SectorEditorTexturePickerApplyResult{};
    }
    const SectorEditorSelectedTexture selected =
            CurrentSectorEditorTexturePickerSelection(context_.texturePicker);
    if (!selected.valid) {
        CloseSectorEditorTexturePicker(context_.texturePicker);
        return SectorEditorTexturePickerApplyResult{};
    }

    SectorEditorMaterialPickerRoutingContext routingContext{
            context_.state,
            context_.statusText,
            [this](const char* status, engine::AssetManager* callbackAssets) {
                return FinishTopologyMaterialMutation(status, callbackAssets);
            },
            [this](
                    TopologySurfaceEditTarget target,
                    const SectorEditorMaterialActionResult& result,
                    const SectorTopologyMap& editedTopology,
                    engine::AssetManager* callbackAssets) {
                return FinishAuthoringSideMaterialActionResult(
                        target,
                        result,
                        editedTopology,
                        callbackAssets);
            },
            [this](
                    TopologySurfaceEditTarget target,
                    engine::AssetManager* callbackAssets,
                    SectorEditorMaterialPickerActionFn action) {
                return ApplyAuthoringFaceAnchorFlatMaterialAction(target, callbackAssets, action);
            },
            [this](const char* status) { MarkTopologyDocumentEdited(status); },
            [this, assets]() {
                return context_.requestPreviewMaterialMeshRebuild
                        ? context_.requestPreviewMaterialMeshRebuild(assets)
                        : false;
            }};
    return ApplySectorEditorMaterialTexturePickerSelection(routingContext, assets);
}

} // namespace game
