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

std::string& FlatTextureIdFor(SectorAuthoringFaceAnchor& anchor, TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            ? anchor.floorTextureId
            : anchor.ceilingTextureId;
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

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    const TopologyMaterialLayer effectiveLayer = EffectiveTopologyMaterialLayer(target.kind, layer);
    SectorEditorMaterialActionResult result;
    context_.state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            context_.state,
            target.sideDefId,
            "Updated authoring side material",
            [&](SectorAuthoringLineSide& side) {
                SectorTopologyWallPartSettings& part = AuthoringWallPartSettingsFor(side, wallPart);
                SectorEditorAuthoringMaterialTarget authoringTarget;
                authoringTarget.target = target;
                authoringTarget.layer = effectiveLayer;
                authoringTarget.wallPart = &part;
                authoringTarget.textureId = effectiveLayer == TopologyMaterialLayer::Decal
                        ? &part.decal.textureId
                        : &part.textureId;
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
    if (!HasAuthoringGraphData(context_.state) || !IsFlatTopologySurfaceTarget(target)) {
        return false;
    }
    if (!action) {
        context_.statusText = "Flat material edit unavailable.";
        return true;
    }

    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string unavailableStatus;
    if (!ResolveSectorEditorAuthoringSurfaceTarget(
                context_.state,
                FlatSurfaceForTarget(context_.state, target),
                authoringTarget,
                &unavailableStatus)
            || authoringTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
        if (context_.state.selectedSurface3D.kind != SectorSurfaceKind::None
                && context_.state.selectedSurface3D.topologySectorId == target.sectorId) {
            context_.state.selectedSurface3D = SectorSurfaceRef{};
            context_.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        }
        context_.statusText = unavailableStatus.empty()
                ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                : unavailableStatus;
        return true;
    }

    SectorEditorMaterialActionResult result;
    context_.state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringFaceAnchorById(
            context_.state,
            authoringTarget.faceAnchorId,
            "Updated authoring face anchor material",
            [&](SectorAuthoringFaceAnchor& anchor) {
                SectorEditorAuthoringMaterialTarget materialTarget;
                materialTarget.target = target;
                materialTarget.layer = layer;
                materialTarget.textureId = layer == TopologyMaterialLayer::Decal
                        ? &FlatDecalFor(anchor, target.kind).textureId
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
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData(context_.state)) {
        return ApplyAuthoringSideMaterialEdit(target, assets, layer, action);
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData(context_.state)) {
        return ApplyAuthoringFaceAnchorMaterialEdit(target, assets, layer, action);
    }
    context_.statusText = HasAuthoringGraphData(context_.state)
            ? "Cannot edit material: selected derived target has no authoring material route."
            : "Cannot edit material: authoring data is required.";
    return false;
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
            TopologyMaterialLayer::Base,
            [this](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.textureId == nullptr || authoringTarget.uv == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return PasteMaterialToFields(
                        authoringTarget.target,
                        context_.state.copiedTopologyMaterial,
                        *authoringTarget.textureId,
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

bool SectorEditorMaterialEditingService::ApplyDecalBloomIntensity(
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            target,
            assets,
            TopologyMaterialLayer::Decal,
            [bloomIntensity](SectorEditorAuthoringMaterialTarget& authoringTarget) {
                if (authoringTarget.decal == nullptr) {
                    return SectorEditorMaterialActionResult{};
                }
                return game::ApplySurfaceDecalBloomIntensityToLayer(
                        authoringTarget.target,
                        bloomIntensity,
                        *authoringTarget.decal);
            });
}

bool SectorEditorMaterialEditingService::OpenDecalTintModal(TopologySurfaceEditTarget target)
{
    if (!HasAuthoringGraphData(context_.state)) {
        context_.statusText = "Cannot edit material: authoring data is required.";
        return false;
    }
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
                        context_.state.topologyMap,
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
                        context_.state.topologyMap,
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
                        context_.state.topologyMap,
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
                        context_.state.topologyMap,
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
                        context_.state.topologyMap,
                        authoringTarget.target,
                        direction,
                        EffectiveTopologyMaterialLayer(authoringTarget.target.kind, layer),
                        *authoringTarget.uv);
            });
}

const SectorTopologyDecalLayer* SectorEditorMaterialEditingService::DecalForSurface(
        TopologySurfaceEditTarget target) const
{
    return game::DecalForMaterialSurface(context_.state.topologyMap, target);
}

const SectorTopologyUvSettings* SectorEditorMaterialEditingService::UvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::UvForMaterialSurface(context_.state.topologyMap, target, layer);
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

bool SectorEditorMaterialEditingService::OpenMaterialPickerForDerivedSector(
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForDerivedSector(
            context_.state,
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
            context_.state,
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
            context_.state,
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
            [this, assets]() {
                return context_.requestPreviewMaterialMeshRebuild
                        ? context_.requestPreviewMaterialMeshRebuild(assets)
                        : false;
            }};
    return ApplySectorEditorMaterialTexturePickerSelection(routingContext, assets);
}

} // namespace game
