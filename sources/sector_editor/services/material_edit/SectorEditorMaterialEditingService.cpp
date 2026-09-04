#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
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

bool IsSelectedSurface3DFlatTarget(
        const SectorEditorPreviewSelectionState& previewSelectionState,
        TopologySurfaceEditTarget target)
{
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
        return previewSelectionState.selectedSurface3D.kind == SectorSurfaceKind::Floor
                && previewSelectionState.selectedSurface3D.topologySectorId == target.sectorId;
    }
    if (target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        return previewSelectionState.selectedSurface3D.kind == SectorSurfaceKind::Ceiling
                && previewSelectionState.selectedSurface3D.topologySectorId == target.sectorId;
    }
    return false;
}

SectorSurfaceRef FlatSurfaceForTarget(
        const SectorEditorPreviewSelectionState& previewSelectionState,
        TopologySurfaceEditTarget target)
{
    SectorSurfaceRef surface = previewSelectionState.selectedSurface3D;
    if (!IsSelectedSurface3DFlatTarget(previewSelectionState, target) && IsFlatTopologySurfaceTarget(target)) {
        surface = SectorSurfaceRef{};
        surface.kind = target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? SectorSurfaceKind::Floor
                : SectorSurfaceKind::Ceiling;
        surface.topologySectorId = target.sectorId;
    }
    return surface;
}

void ResetSurface3DUi(MaterialEditingUiState& materialUiState)
{
    materialUiState.surface3DUvScaleUInput = engine::UIFloatInputState{};
    materialUiState.surface3DUvScaleVInput = engine::UIFloatInputState{};
    materialUiState.surface3DUvOffsetUInput = engine::UIFloatInputState{};
    materialUiState.surface3DUvOffsetVInput = engine::UIFloatInputState{};
    materialUiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
    materialUiState.surface3DDecalEmissiveStrengthInput = engine::UIFloatInputState{};
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

std::string& FlatTextureIdFor(SectorAuthoringFaceAnchor& anchor, TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            ? anchor.floorMaterialId
            : anchor.ceilingMaterialId;
}

SectorTopologyUvSettings& FlatUvFor(SectorAuthoringFaceAnchor& anchor, TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            ? anchor.floorUv
            : anchor.ceilingUv;
}

SectorTopologyDecalLayer& FlatDecalFor(SectorAuthoringFaceAnchor& anchor, TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            ? anchor.floorDecal
            : anchor.ceilingDecal;
}

} // namespace

SectorEditorMaterialEditingService::SectorEditorMaterialEditingService(
        SectorEditorMaterialEditingServiceContext context)
    : context_(std::move(context))
{
}

void SectorEditorMaterialEditingService::MarkTopologyDocumentEdited(const char* status)
{
    context_.lifecycle.topologyDocumentDirty = true;
    context_.lifecycle.hasUnsavedChanges = true;
    ++context_.topologyRenderRevision;
    context_.topologyRenderCache.valid = false;
    if (status != nullptr && status[0] != '\0') {
        context_.statusText = status;
    }
}

void SectorEditorMaterialEditingService::ApplyMaterialUiResetFlags(
        const SectorEditorMaterialActionResult& result)
{
    if (result.resetSurface3DUi) {
        ResetSurface3DUi(context_.materialUiState);
    }
    if (result.resetSectorUvInputs) {
        for (engine::UIFloatInputState& inputState : context_.materialUiState.topologySectorUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetSideDefUvInputs) {
        for (engine::UIFloatInputState& inputState : context_.materialUiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetDecalInputs) {
        for (engine::UIFloatInputState& inputState : context_.materialUiState.topologySectorDecalOpacityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        for (engine::UIFloatInputState& inputState : context_.materialUiState.topologySectorDecalEmissiveStrengthInputs) {
            inputState = engine::UIFloatInputState{};
        }
        context_.materialUiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        context_.materialUiState.topologySideDefDecalEmissiveStrengthInput = engine::UIFloatInputState{};
        context_.materialUiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
        context_.materialUiState.surface3DDecalEmissiveStrengthInput = engine::UIFloatInputState{};
    }
    if (result.closeDecalTintModal) {
        context_.decalTintModal = DecalTintModalState{};
    }
}

bool SectorEditorMaterialEditingService::FinishAuthoringMaterialActionResult(
        const SectorEditorMaterialActionResult& result,
        bool refreshed,
        engine::AssetManager* assets,
        const char* failureStatus)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            context_.statusText = result.status;
        }
        return false;
    }

    ApplyMaterialUiResetFlags(result);
    if (!refreshed) {
        context_.statusText = failureStatus == nullptr ? "Cannot edit material." : failureStatus;
    } else if (!result.status.empty()) {
        context_.statusText = result.status;
    }
    if (assets != nullptr && refreshed && context_.requestPreviewMaterialMeshRebuild) {
        return context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return refreshed;
}

bool SectorEditorMaterialEditingService::ApplyAuthoringSideMaterialEdit(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer,
        const SectorEditorAuthoringMaterialActionFn& action)
{
    if (!IsWallTopologyEditTarget(target.kind) || !HasAuthoringGraphData(context_.authoringGraph)) {
        return false;
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Wall material edit unavailable: derived topology is not current";
        return true;
    }
    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
                target.sideDefId,
                sideId)) {
        context_.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
        return true;
    }
    if (!action) {
        return false;
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    const TopologyMaterialLayer effectiveLayer = EffectiveTopologyMaterialLayer(target.kind, layer);
    SectorEditorMaterialActionResult result;
    context_.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            target.sideDefId,
            "Updated authoring side material",
            [&](SectorAuthoringLineSide& side) {
                SectorTopologyWallPartSettings& part = AuthoringWallPartSettingsFor(side, wallPart);
                SectorEditorAuthoringMaterialTarget authoringTarget;
                authoringTarget.target = target;
                authoringTarget.layer = effectiveLayer;
                authoringTarget.wallPart = &part;
                authoringTarget.materialId = effectiveLayer == TopologyMaterialLayer::Decal
                        ? &part.decal.materialId
                        : &part.materialId;
                authoringTarget.uv = effectiveLayer == TopologyMaterialLayer::Decal
                        ? &part.decal.uv
                        : &part.uv;
                authoringTarget.decal = effectiveLayer == TopologyMaterialLayer::Decal
                        ? &part.decal
                        : nullptr;
                result = action(authoringTarget);
                return result.changed;
            });
    return FinishAuthoringMaterialActionResult(
            result,
            refreshed,
            assets,
            "Wall material edit unavailable: selected sidedef has no authoring side mapping");
}

bool SectorEditorMaterialEditingService::ApplyAuthoringFaceAnchorMaterialEdit(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer,
        const SectorEditorAuthoringMaterialActionFn& action)
{
    if (!HasAuthoringGraphData(context_.authoringGraph) || !IsFlatTopologySurfaceTarget(target)) {
        return false;
    }
    if (!action) {
        context_.statusText = "Flat material edit unavailable.";
        return true;
    }

    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string unavailableStatus;
    if (!ResolveSectorEditorAuthoringSurfaceTarget(
                context_.topologyMap,
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
                IsSectorEditorAuthoringDerivationCurrent(context_.derivation),
                FlatSurfaceForTarget(context_.previewSelectionState, target),
                authoringTarget,
                &unavailableStatus)
            || authoringTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
        if (context_.previewSelectionState.selectedSurface3D.kind != SectorSurfaceKind::None
                && context_.previewSelectionState.selectedSurface3D.topologySectorId == target.sectorId) {
            context_.previewSelectionState.selectedSurface3D = SectorSurfaceRef{};
            context_.previewSelectionState.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        }
        context_.statusText = unavailableStatus.empty()
                ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                : unavailableStatus;
        return true;
    }

    SectorEditorMaterialActionResult result;
    context_.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringFaceAnchorById(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            authoringTarget.faceAnchorId,
            "Updated authoring face anchor material",
            [&](SectorAuthoringFaceAnchor& anchor) {
                SectorEditorAuthoringMaterialTarget materialTarget;
                materialTarget.target = target;
                materialTarget.layer = layer;
                materialTarget.materialId = layer == TopologyMaterialLayer::Decal
                        ? &FlatDecalFor(anchor, target.kind).materialId
                        : &FlatTextureIdFor(anchor, target.kind);
                materialTarget.uv = layer == TopologyMaterialLayer::Decal
                        ? &FlatDecalFor(anchor, target.kind).uv
                        : &FlatUvFor(anchor, target.kind);
                materialTarget.decal = layer == TopologyMaterialLayer::Decal
                        ? &FlatDecalFor(anchor, target.kind)
                        : nullptr;
                result = action(materialTarget);
                return result.changed;
            });
    return FinishAuthoringMaterialActionResult(
            result,
            refreshed,
            assets,
            "3D flat surface edit unavailable: selected sector has no face anchor mapping");
}

bool SectorEditorMaterialEditingService::ApplyMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer,
        const SectorEditorAuthoringMaterialActionFn& action)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData(context_.authoringGraph)) {
        return ApplyAuthoringSideMaterialEdit(target, assets, layer, action);
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData(context_.authoringGraph)) {
        return ApplyAuthoringFaceAnchorMaterialEdit(target, assets, layer, action);
    }
    context_.statusText = HasAuthoringGraphData(context_.authoringGraph)
            ? "Cannot edit material: selected derived target has no authoring material route."
            : "Cannot edit material: authoring data is required.";
    return false;
}

bool SectorEditorMaterialEditingService::CopyMaterial(
        TopologySurfaceEditTarget target,
        TopologyMaterialPayload& outPayload)
{
    TopologyMaterialPayload payload;
    std::string status;
    if (!CopyMaterialSurface(context_.topologyMap, target, payload, status)) {
        context_.statusText = status;
        return false;
    }
    outPayload = std::move(payload);
    context_.statusText = status;
    return true;
}

bool SectorEditorMaterialEditingService::PasteMaterial(
        TopologySurfaceEditTarget target,
        const TopologyMaterialPayload& payload,
        engine::AssetManager& assets)
{
    return ApplyMaterialAction(
            target,
            &assets,
            TopologyMaterialLayer::Base,
            [&payload](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.materialId == nullptr || authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return PasteMaterialToFields(
                        authoringTarget.target,
                        payload,
                        *authoringTarget.materialId,
                        *authoringTarget.uv);
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
            layer,
            [layer, component, value, surfaceKind](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceUvValueToSettings(
                        authoringTarget.target,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        surfaceKind,
                        component,
                        value,
                        *authoringTarget.uv);
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
            layer,
            [layer, surfaceKind](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ResetSurfaceUvSettings(
                        authoringTarget.target,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        surfaceKind,
                        *authoringTarget.uv);
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
            UvForMaterialSurface(context_.topologyMap, target, effectiveLayer);
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

    if (!HasAuthoringGraphData(context_.authoringGraph)) {
        context_.statusText = "Cannot edit sidedef UV: material UV editor requires authoring data.";
        return false;
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Cannot edit sidedef UV: derived topology is not current.";
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
                target.sideDefId,
                sideId)) {
        context_.statusText = "Cannot edit sidedef UV: selected topology side is not mapped to an authoring line.";
        return false;
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    context_.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
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
            UvForMaterialSurface(context_.topologyMap, target, effectiveLayer);
    if (derivedUv == nullptr) {
        context_.statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }

    if (!HasAuthoringGraphData(context_.authoringGraph)) {
        context_.statusText = "Cannot edit sidedef UV: material UV editor requires authoring data.";
        return false;
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Cannot edit sidedef UV: derived topology is not current.";
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
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
    context_.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
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
    for (engine::UIFloatInputState& inputState : context_.materialUiState.topologySideDefUvInputs) {
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
            TopologyMaterialLayer::Decal,
            [opacity](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceDecalOpacityToLayer(
                        authoringTarget.target,
                        opacity,
                        *authoringTarget.decal);
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
            TopologyMaterialLayer::Decal,
            [emissive](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceDecalEmissiveToLayer(
                        authoringTarget.target,
                        emissive,
                        *authoringTarget.decal);
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
            TopologyMaterialLayer::Decal,
            [tint](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceDecalTintToLayer(
                        authoringTarget.target,
                        tint,
                        *authoringTarget.decal);
            });
}

bool SectorEditorMaterialEditingService::ApplyDecalEmissiveStrength(
        TopologySurfaceEditTarget target,
        float emissiveStrength,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Decal,
            [emissiveStrength](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceDecalEmissiveStrengthToLayer(
                        authoringTarget.target,
                        emissiveStrength,
                        *authoringTarget.decal);
            });
}

bool SectorEditorMaterialEditingService::OpenDecalTintModal(TopologySurfaceEditTarget target)
{
    if (!HasAuthoringGraphData(context_.authoringGraph)) {
        context_.statusText = "Cannot edit material: authoring data is required.";
        return false;
    }
    DecalTintModalState modal;
    std::string status;
    if (!BuildDecalTintModal(context_.topologyMap, target, modal, status)) {
        context_.statusText = status;
        return false;
    }
    context_.decalTintModal = modal;
    return true;
}

bool SectorEditorMaterialEditingService::ClearSurfaceDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Decal,
            [](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ClearSurfaceDecalLayer(authoringTarget.target, *authoringTarget.decal);
            });
}

bool SectorEditorMaterialEditingService::ClearMiddleTexture(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Base,
            [](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.wallPart == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ClearMiddleTextureSettings(
                        authoringTarget.target,
                        *authoringTarget.wallPart);
            });
}

bool SectorEditorMaterialEditingService::UseDefaultSurfaceMaterial(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (target.kind == TopologySurfaceEditTargetKind::SideDefMiddle) {
        return false;
    }
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Base,
            [](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                SectorEditorMaterialActionResult result;
                if (authoringTarget.materialId == nullptr
                        || authoringTarget.materialId->empty()) {
                    return result;
                }
                authoringTarget.materialId->clear();
                result.changed = true;
                result.status = "Using built-in default material.";
                return result;
            });
}

bool SectorEditorMaterialEditingService::UseDefaultAuthoringFaceMaterial(
        int faceAnchorId,
        TopologySectorTextureField field,
        engine::AssetManager* assets)
{
    const bool changed = MutateSectorEditorAuthoringFaceAnchorById(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            faceAnchorId,
            "Using built-in default material",
            [field](SectorAuthoringFaceAnchor& anchor) {
                std::string* materialId = nullptr;
                switch (field) {
                    case TopologySectorTextureField::Floor:
                        materialId = &anchor.floorMaterialId;
                        break;
                    case TopologySectorTextureField::Ceiling:
                        materialId = &anchor.ceilingMaterialId;
                        break;
                    case TopologySectorTextureField::DefaultWall:
                        materialId = &anchor.defaultWall.materialId;
                        break;
                    case TopologySectorTextureField::DefaultLower:
                        materialId = &anchor.defaultLower.materialId;
                        break;
                    case TopologySectorTextureField::DefaultUpper:
                        materialId = &anchor.defaultUpper.materialId;
                        break;
                }
                if (materialId == nullptr || materialId->empty()) return false;
                materialId->clear();
                return true;
            });
    if (changed && assets != nullptr && context_.requestPreviewMaterialMeshRebuild) {
        context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return changed;
}

bool SectorEditorMaterialEditingService::UseDefaultAuthoringSideMaterial(
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        engine::AssetManager* assets)
{
    if (wallPart == TopologyWallPart::Middle) return false;
    const bool changed = MutateSectorEditorAuthoringSideById(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            sideId,
            "Using built-in default material",
            [wallPart](SectorAuthoringLineSide& side) {
                std::string& materialId =
                        AuthoringWallPartSettingsFor(side, wallPart).materialId;
                if (materialId.empty()) return false;
                materialId.clear();
                return true;
            });
    if (changed && assets != nullptr && context_.requestPreviewMaterialMeshRebuild) {
        context_.requestPreviewMaterialMeshRebuild(assets);
    }
    return changed;
}

bool SectorEditorMaterialEditingService::FitSelectedDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Decal,
            [this](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::FitSelectedDecalToAuthoring(
                        context_.topologyMap,
                        authoringTarget.target,
                        *authoringTarget.uv);
            });
}

bool SectorEditorMaterialEditingService::FitSelectedFlatDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Decal,
            [this](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::FitSelectedFlatDecalToUv(
                        context_.topologyMap,
                        authoringTarget.target,
                        *authoringTarget.uv);
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
            layer,
            [this, mode, layer](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::FitSelectedWallMaterialToUv(
                        context_.topologyMap,
                        authoringTarget.target,
                        mode,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        *authoringTarget.uv);
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
            layer,
            [this, layer](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::AlignSelectedWallMaterialVerticalToUv(
                        context_.topologyMap,
                        authoringTarget.target,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        *authoringTarget.uv);
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
            layer,
            [this, direction, layer](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::AlignSelectedWallMaterialUToUv(
                        context_.topologyMap,
                        authoringTarget.target,
                        direction,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        *authoringTarget.uv);
            });
}

const SectorTopologyDecalLayer* SectorEditorMaterialEditingService::DecalForSurface(
        TopologySurfaceEditTarget target) const
{
    return game::DecalForMaterialSurface(context_.topologyMap, target);
}

const SectorTopologyUvSettings* SectorEditorMaterialEditingService::UvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::UvForMaterialSurface(context_.topologyMap, target, layer);
}

bool SectorEditorMaterialEditingService::IsDecalAssigned(TopologySurfaceEditTarget target) const
{
    return game::IsMaterialDecalAssigned(context_.topologyMap, target);
}

bool SectorEditorMaterialEditingService::IsValidSurfaceTarget(TopologySurfaceEditTarget target) const
{
    return game::IsValidMaterialSurfaceTarget(context_.topologyMap, target);
}

std::string SectorEditorMaterialEditingService::CurrentTextureForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::CurrentTextureForMaterialSurface(context_.topologyMap, target, layer);
}

std::string SectorEditorMaterialEditingService::CurrentTextureForPickerTarget() const
{
    if (context_.texturePicker.topologyTargetKind
            == TopologyTexturePickerTargetKind::AuthoringStructuralPrimitive) {
        const SectorAuthoringStructuralPrimitive* primitive =
                FindSectorAuthoringStructuralPrimitive(
                        context_.authoringGraph,
                        context_.texturePicker.authoringStructuralPrimitiveId);
        if (primitive == nullptr) return {};
        const int group = context_.texturePicker.authoringStructuralSurfaceGroup;
        return group < 0
                ? primitive->materials.defaultSurface.materialId
                : primitive->materials.overrides[static_cast<size_t>(group)]
                        .settings.materialId;
    }
    return CurrentSectorEditorMaterialPickerTexture(
            context_.topologyMap,
            context_.authoringGraph,
            game::MakeSectorEditorConstDerivationDocumentAccess(context_.derivation),
            context_.texturePicker);
}

bool SectorEditorMaterialEditingService::OpenMaterialPickerForDerivedSector(
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForDerivedSector(
            context_.texturePicker,
            SortedSectorMaterialIds(context_.materialRegistry),
            context_.topologyMap,
            context_.authoringGraph,
            game::MakeSectorEditorConstDerivationDocumentAccess(context_.derivation),
            topologySectorId,
            field,
            layer);
}

bool SectorEditorMaterialEditingService::OpenMaterialPickerForAuthoringFaceAnchor(
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
            context_.texturePicker,
            SortedSectorMaterialIds(context_.materialRegistry),
            context_.topologyMap,
            context_.authoringGraph,
            game::MakeSectorEditorConstDerivationDocumentAccess(context_.derivation),
            faceAnchorId,
            field,
            layer);
}

bool SectorEditorMaterialEditingService::OpenMaterialPickerForDerivedSideDef(
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForDerivedSideDef(
            context_.texturePicker,
            SortedSectorMaterialIds(context_.materialRegistry),
            context_.topologyMap,
            context_.authoringGraph,
            game::MakeSectorEditorConstDerivationDocumentAccess(context_.derivation),
            topologySideDefId,
            wallPart,
            layer);
}

bool SectorEditorMaterialEditingService::OpenMaterialPickerForAuthoringSide(
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSide(
            context_.texturePicker,
            SortedSectorMaterialIds(context_.materialRegistry),
            context_.topologyMap,
            context_.authoringGraph,
            game::MakeSectorEditorConstDerivationDocumentAccess(context_.derivation),
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

    if (context_.texturePicker.topologyTargetKind
            == TopologyTexturePickerTargetKind::AuthoringStructuralPrimitive) {
        SectorEditorTexturePickerApplyResult result;
        const int primitiveId = context_.texturePicker.authoringStructuralPrimitiveId;
        const int group = context_.texturePicker.authoringStructuralSurfaceGroup;
        result.changed = MutateAuthoringStructuralPrimitive(
                primitiveId,
                "Updated structure material",
                [group, &selected](SectorAuthoringStructuralPrimitive& primitive) {
                    SectorStructuralMaterialSettings* settings =
                            &primitive.materials.defaultSurface;
                    if (group >= 0
                            && group < static_cast<int>(SectorStructuralSurfaceGroup::Count)) {
                        settings = &primitive.materials.overrides[
                                static_cast<size_t>(group)].settings;
                    }
                    if (settings->materialId == selected.materialId) return false;
                    settings->materialId = selected.materialId;
                    return true;
                });
        result.status = result.changed
                ? "Updated structure material" : "Structure material unchanged";
        context_.statusText = result.status;
        CloseSectorEditorTexturePicker(context_.texturePicker);
        if (result.changed && assets != nullptr
                && context_.requestPreviewMaterialMeshRebuild) {
            context_.requestPreviewMaterialMeshRebuild(assets);
        }
        return result;
    }

    SectorEditorMaterialPickerRoutingContext routingContext{
            context_.texturePicker,
            context_.lifecycle,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.statusText,
            [this, assets]() {
                return context_.requestPreviewMaterialMeshRebuild
                        ? context_.requestPreviewMaterialMeshRebuild(assets)
                        : false;
            }};
    return ApplySectorEditorMaterialTexturePickerSelection(routingContext, assets);
}

bool SectorEditorMaterialEditingService::OpenMaterialPickerForAuthoringStructuralPrimitive(
        int primitiveId,
        int surfaceGroup)
{
    const SectorAuthoringStructuralPrimitive* primitive =
            FindSectorAuthoringStructuralPrimitive(context_.authoringGraph, primitiveId);
    if (primitive == nullptr || surfaceGroup < -1
            || surfaceGroup >= static_cast<int>(SectorStructuralSurfaceGroup::Count)) {
        CloseSectorEditorTexturePicker(context_.texturePicker);
        return false;
    }
    context_.texturePicker.rebuildPreviewOnApply = false;
    context_.texturePicker.authoringSurface3DFlatTarget = false;
    context_.texturePicker.topologyTargetKind =
            TopologyTexturePickerTargetKind::AuthoringStructuralPrimitive;
    context_.texturePicker.authoringStructuralPrimitiveId = primitiveId;
    context_.texturePicker.authoringStructuralSurfaceGroup = surfaceGroup;
    const SectorStructuralMaterialSettings& settings = surfaceGroup < 0
            ? primitive->materials.defaultSurface
            : primitive->materials.overrides[static_cast<size_t>(surfaceGroup)].settings;
    OpenSectorEditorTexturePicker(
            context_.texturePicker,
            SortedSectorMaterialIds(context_.materialRegistry),
            settings.materialId);
    return true;
}

bool SectorEditorMaterialEditingService::ApplyAuthoringStructuralPrimitiveUvValue(
        int primitiveId,
        int surfaceGroup,
        int component,
        float value)
{
    if (!std::isfinite(value) || component < 0 || component > 3
            || surfaceGroup < -1
            || surfaceGroup >= static_cast<int>(SectorStructuralSurfaceGroup::Count)
            || (component < 2 && value == 0.0f)) return false;
    return MutateAuthoringStructuralPrimitive(
            primitiveId,
            "Updated structure material UV",
            [surfaceGroup, component, value](SectorAuthoringStructuralPrimitive& primitive) {
                SectorTopologyUvSettings& uv = surfaceGroup < 0
                        ? primitive.materials.defaultSurface.uv
                        : primitive.materials.overrides[
                                static_cast<size_t>(surfaceGroup)].settings.uv;
                float* target = component == 0 ? &uv.scale.x
                        : component == 1 ? &uv.scale.y
                        : component == 2 ? &uv.offset.x : &uv.offset.y;
                if (*target == value) return false;
                *target = value;
                return true;
            });
}

bool SectorEditorMaterialEditingService::UseDefaultAuthoringStructuralPrimitiveMaterial(
        int primitiveId,
        int surfaceGroup)
{
    if (surfaceGroup < -1
            || surfaceGroup >= static_cast<int>(SectorStructuralSurfaceGroup::Count)) {
        return false;
    }
    return MutateAuthoringStructuralPrimitive(
            primitiveId,
            "Using built-in default structure material",
            [surfaceGroup](SectorAuthoringStructuralPrimitive& primitive) {
                SectorStructuralMaterialSettings& settings = surfaceGroup < 0
                        ? primitive.materials.defaultSurface
                        : primitive.materials.overrides[
                                static_cast<size_t>(surfaceGroup)].settings;
                if (settings.materialId.empty()) return false;
                settings.materialId.clear();
                return true;
            });
}

bool SectorEditorMaterialEditingService::SetAuthoringStructuralMaterialOverrideEnabled(
        int primitiveId,
        SectorStructuralSurfaceGroup group,
        bool enabled)
{
    if (group == SectorStructuralSurfaceGroup::Count) return false;
    return MutateAuthoringStructuralPrimitive(
            primitiveId,
            "Updated structure material override",
            [group, enabled](SectorAuthoringStructuralPrimitive& primitive) {
                auto& target = primitive.materials.overrides[static_cast<size_t>(group)];
                if (target.enabled == enabled) return false;
                if (enabled) target.settings = primitive.materials.defaultSurface;
                target.enabled = enabled;
                return true;
            });
}

bool SectorEditorMaterialEditingService::MutateAuthoringStructuralPrimitive(
        int primitiveId,
        const char* status,
        const std::function<bool(SectorAuthoringStructuralPrimitive&)>& mutate)
{
    if (!mutate) return false;
    SectorAuthoringGraph candidate = context_.authoringGraph;
    SectorAuthoringStructuralPrimitive* primitive =
            FindSectorAuthoringStructuralPrimitive(candidate, primitiveId);
    if (primitive == nullptr || !mutate(*primitive)) return false;
    SectorAuthoringDerivationResult candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidate);
    if (!candidateDerivation.success) {
        context_.statusText = "Structure material edit rejected by authoring derivation";
        return false;
    }
    context_.authoringGraph = std::move(candidate);
    MarkSectorEditorAuthoringGraphEdited(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.derivation,
            status);
    return RefreshSectorEditorAuthoringDerivation(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            status,
            "Updated structure material; derivation failed");
}

} // namespace game
