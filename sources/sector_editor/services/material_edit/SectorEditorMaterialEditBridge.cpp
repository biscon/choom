#include "sector_editor/services/material_edit/SectorEditorMaterialEditBridge.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"

namespace game {

namespace {

bool IsFlatTopologySurfaceTarget(TopologySurfaceEditTarget target)
{
    return target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

bool FinishMaterialAction(
        SectorEditorMaterialEditBridgeContext& context,
        const SectorEditorMaterialActionResult& result,
        engine::AssetManager* assets)
{
    if (!context.callbacks.finishMaterialActionResult) {
        return false;
    }
    return context.callbacks.finishMaterialActionResult(result, assets);
}

bool ApplyMaterialAction(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const SectorEditorMaterialEditActionFn& action)
{
    if (IsWallTopologyEditTarget(target.kind)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringSideMaterialAction) {
        return context.callbacks.applyAuthoringSideMaterialAction(target, assets, action);
    }
    if (IsFlatTopologySurfaceTarget(target)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringFaceAnchorFlatMaterialAction) {
        return context.callbacks.applyAuthoringFaceAnchorFlatMaterialAction(target, assets, action);
    }
    return FinishMaterialAction(context, action(context.state.topologyMap), assets);
}

} // namespace

bool CopySectorEditorMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target)
{
    TopologyMaterialPayload payload;
    std::string status;
    if (!CopyMaterialSurface(context.state.topologyMap, target, payload, status)) {
        context.statusText = status;
        return false;
    }
    context.state.copiedTopologyMaterial = payload;
    context.statusText = status;
    return true;
}

bool PasteSectorEditorMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager& assets)
{
    return ApplyMaterialAction(
            context,
            target,
            &assets,
            [&context, target](SectorTopologyMap& map) {
                return PasteMaterialSurface(map, target, context.state.copiedTopologyMaterial);
            });
}

bool ApplySectorEditorSurfaceDecalOpacity(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        float opacity,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target, opacity](SectorTopologyMap& map) {
                return ApplySurfaceDecalOpacity(map, target, opacity);
            });
}

bool ApplySectorEditorSurfaceDecalEmissive(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target, emissive](SectorTopologyMap& map) {
                return ApplySurfaceDecalEmissive(map, target, emissive);
            });
}

bool ApplySectorEditorSurfaceDecalTint(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target, tint](SectorTopologyMap& map) {
                return ApplySurfaceDecalTint(map, target, tint);
            });
}

bool ApplySectorEditorSurfaceDecalBloomIntensity(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target, bloomIntensity](SectorTopologyMap& map) {
                return ApplySurfaceDecalBloomIntensity(map, target, bloomIntensity);
            });
}

bool OpenSectorEditorDecalTintModal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target)
{
    DecalTintModalState modal;
    std::string status;
    if (!BuildDecalTintModal(context.state.topologyMap, target, modal, status)) {
        context.statusText = status;
        return false;
    }
    context.state.decalTintModal = modal;
    return true;
}

bool ClearSectorEditorSurfaceDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return ClearSurfaceDecal(map, target);
            });
}

bool ClearSectorEditorMiddleTexture(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringSideMaterialAction) {
        return context.callbacks.applyAuthoringSideMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return ClearMiddleTexture(map, target);
                });
    }
    return FinishMaterialAction(context, ClearMiddleTexture(context.state.topologyMap, target), assets);
}

bool FitSectorEditorSelectedDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return ApplyMaterialAction(
            context,
            target,
            assets,
            [target](SectorTopologyMap& map) {
                return FitSelectedDecal(map, target);
            });
}

bool FitSectorEditorSelectedFlatDecal(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsFlatTopologySurfaceTarget(target)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringFaceAnchorFlatMaterialAction) {
        return context.callbacks.applyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return FitSelectedFlatDecal(map, target);
                });
    }
    return FinishMaterialAction(context, FitSelectedFlatDecal(context.state.topologyMap, target), assets);
}

bool FitSectorEditorSelectedWallMaterial(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringSideMaterialAction) {
        return context.callbacks.applyAuthoringSideMaterialAction(
                target,
                assets,
                [target, mode, layer](SectorTopologyMap& map) {
                    return FitSelectedWallMaterial(map, target, mode, layer);
                });
    }
    return FinishMaterialAction(
            context,
            FitSelectedWallMaterial(context.state.topologyMap, target, mode, layer),
            assets);
}

bool AlignSectorEditorSelectedWallMaterialVertical(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringSideMaterialAction) {
        return context.callbacks.applyAuthoringSideMaterialAction(
                target,
                assets,
                [target, layer](SectorTopologyMap& map) {
                    return AlignSelectedWallMaterialVertical(map, target, layer);
                });
    }
    return FinishMaterialAction(
            context,
            AlignSelectedWallMaterialVertical(context.state.topologyMap, target, layer),
            assets);
}

bool AlignSectorEditorSelectedWallMaterialU(
        SectorEditorMaterialEditBridgeContext& context,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind)
            && HasAuthoringGraphData(context.state)
            && context.callbacks.applyAuthoringSideMaterialAction) {
        return context.callbacks.applyAuthoringSideMaterialAction(
                target,
                assets,
                [target, direction, layer](SectorTopologyMap& map) {
                    return AlignSelectedWallMaterialU(map, target, direction, layer);
                });
    }
    return FinishMaterialAction(
            context,
            AlignSelectedWallMaterialU(context.state.topologyMap, target, direction, layer),
            assets);
}

} // namespace game
