#include "sector_editor/services/config_clipboard/SectorEditorConfigClipboardService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"

#include <utility>

namespace game {
namespace {

bool SameVector2(Vector2 a, Vector2 b)
{
    return a.x == b.x && a.y == b.y;
}

bool SameVector3(Vector3 a, Vector3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool SameUv(const SectorTopologyUvSettings& a, const SectorTopologyUvSettings& b)
{
    return SameVector2(a.scale, b.scale) && SameVector2(a.offset, b.offset);
}

bool SameDecal(const SectorTopologyDecalLayer& a, const SectorTopologyDecalLayer& b)
{
    return a.materialId == b.materialId
            && SameUv(a.uv, b.uv)
            && a.opacity == b.opacity
            && a.emissive == b.emissive
            && SameVector3(a.tint, b.tint)
            && a.bloomIntensity == b.bloomIntensity;
}

bool SameWallPart(
        const SectorTopologyWallPartSettings& a,
        const SectorTopologyWallPartSettings& b)
{
    return a.materialId == b.materialId
            && SameUv(a.uv, b.uv)
            && SameDecal(a.decal, b.decal);
}

bool SameSectorConfig(
        const SectorAuthoringFaceAnchor& a,
        const SectorAuthoringFaceAnchor& b)
{
    return a.isVoid == b.isVoid
            && a.floorZ == b.floorZ
            && a.ceilingZ == b.ceilingZ
            && a.floorMaterialId == b.floorMaterialId
            && a.ceilingMaterialId == b.ceilingMaterialId
            && a.footstepSet == b.footstepSet
            && a.ceilingSky == b.ceilingSky
            && a.roomtone.mode == b.roomtone.mode
            && a.roomtone.soundId == b.roomtone.soundId
            && a.roomtone.volume == b.roomtone.volume
            && a.roomtone.fadeMilliseconds == b.roomtone.fadeMilliseconds
            && SameUv(a.floorUv, b.floorUv)
            && SameUv(a.ceilingUv, b.ceilingUv)
            && SameDecal(a.floorDecal, b.floorDecal)
            && SameDecal(a.ceilingDecal, b.ceilingDecal)
            && SameColor(a.ambientColor, b.ambientColor)
            && a.ambientIntensity == b.ambientIntensity
            && SameWallPart(a.defaultWall, b.defaultWall)
            && SameWallPart(a.defaultLower, b.defaultLower)
            && SameWallPart(a.defaultUpper, b.defaultUpper);
}

bool IsLightKind(SectorEditorConfigKind kind)
{
    return kind == SectorEditorConfigKind::StaticPointLight
            || kind == SectorEditorConfigKind::StaticSpotLight
            || kind == SectorEditorConfigKind::StaticRectLight
            || kind == SectorEditorConfigKind::DynamicPointLight
            || kind == SectorEditorConfigKind::DynamicSpotLight
            || kind == SectorEditorConfigKind::DynamicRectLight;
}

bool IsSurfaceKind(SectorEditorConfigKind kind)
{
    return kind == SectorEditorConfigKind::SurfaceFloor
            || kind == SectorEditorConfigKind::SurfaceCeiling
            || kind == SectorEditorConfigKind::SurfaceWall
            || kind == SectorEditorConfigKind::SurfaceLower
            || kind == SectorEditorConfigKind::SurfaceUpper;
}

bool IsRuntimeObjectKind(SectorEditorConfigKind kind)
{
    return kind == SectorEditorConfigKind::Door
            || kind == SectorEditorConfigKind::StaticModel
            || kind == SectorEditorConfigKind::DynamicModel;
}

bool ValidLightTarget(
        const SectorTopologyMap& map,
        const SelectionState& selection,
        SectorEditorConfigTarget& outTarget)
{
    const int staticPointId = selection.selectedTopologyLightId;
    const int staticAimedId = selection.selectedTopologyStaticSpotLightId;
    const int dynamicPointId = selection.selectedTopologyDynamicLightId;
    const int dynamicAimedId = selection.selectedTopologyDynamicSpotLightId;
    switch (selection.topologySelectionKind) {
        case TopologySelectionKind::StaticLight:
            if (FindSectorTopologyStaticLight(map, staticPointId) != nullptr) {
                outTarget = {SectorEditorConfigKind::StaticPointLight, staticPointId};
                return true;
            }
            break;
        case TopologySelectionKind::StaticSpotLight:
            if (FindSectorTopologyStaticSpotLight(map, staticAimedId) != nullptr) {
                outTarget = {SectorEditorConfigKind::StaticSpotLight, staticAimedId};
                return true;
            }
            break;
        case TopologySelectionKind::StaticRectLight:
            if (FindSectorTopologyStaticRectLight(map, staticAimedId) != nullptr) {
                outTarget = {SectorEditorConfigKind::StaticRectLight, staticAimedId};
                return true;
            }
            break;
        case TopologySelectionKind::DynamicLight:
            if (FindSectorTopologyDynamicLight(map, dynamicPointId) != nullptr) {
                outTarget = {SectorEditorConfigKind::DynamicPointLight, dynamicPointId};
                return true;
            }
            break;
        case TopologySelectionKind::DynamicSpotLight:
            if (FindSectorTopologyDynamicSpotLight(map, dynamicAimedId) != nullptr) {
                outTarget = {SectorEditorConfigKind::DynamicSpotLight, dynamicAimedId};
                return true;
            }
            break;
        case TopologySelectionKind::DynamicRectLight:
            if (FindSectorTopologyDynamicRectLight(map, dynamicAimedId) != nullptr) {
                outTarget = {SectorEditorConfigKind::DynamicRectLight, dynamicAimedId};
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

} // namespace

SectorEditorConfigTarget ResolveSectorEditorConfigTarget(
        SectorEditorMode mode,
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        const SelectionState& selectionState,
        const SectorEditorPreviewSelectionState& previewSelectionState)
{
    if (mode == SectorEditorMode::Preview3D) {
        const TopologySurfaceEditTarget surface =
                previewSelectionState.selectedTopologySurface3D;
        const SectorEditorConfigKind surfaceKind =
                SectorEditorSurfaceConfigKind(surface.kind);
        if (surfaceKind != SectorEditorConfigKind::None
                && IsValidMaterialSurfaceTarget(map, surface)) {
            SectorEditorConfigTarget target;
            target.kind = surfaceKind;
            target.surface = surface;
            return target;
        }
    }

    SectorEditorConfigTarget target;
    if (ValidLightTarget(map, selectionState, target)) {
        return target;
    }

    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            map,
            selectionState.selectedRuntimeObjectId);
    if (object != nullptr) {
        if (object->kind == "door") {
            return SectorEditorConfigTarget{SectorEditorConfigKind::Door, object->id};
        }
        if (object->kind == "static_model") {
            return SectorEditorConfigTarget{
                    SectorEditorConfigKind::StaticModel,
                    object->id};
        }
        if (object->kind == "dynamic_model") {
            return SectorEditorConfigTarget{
                    SectorEditorConfigKind::DynamicModel,
                    object->id};
        }
    }

    if (selectionState.selectedAuthoring.kind
                    == SectorAuthoringSelectionKind::FaceAnchor
            && FindSectorAuthoringFaceAnchor(
                    authoringGraph,
                    selectionState.selectedAuthoring.faceAnchorId) != nullptr) {
        return SectorEditorConfigTarget{
                SectorEditorConfigKind::Sector,
                selectionState.selectedAuthoring.faceAnchorId};
    }

    if (selectionState.topologySelectionKind == TopologySelectionKind::Sector
            && IsSectorEditorAuthoringDerivationCurrent(derivation)) {
        const int faceAnchorId =
                FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                        authoringGraph,
                        derivation.authoringDerivation,
                        selectionState.selectedTopologySectorId);
        if (FindSectorAuthoringFaceAnchor(authoringGraph, faceAnchorId) != nullptr) {
            return SectorEditorConfigTarget{
                    SectorEditorConfigKind::Sector,
                    faceAnchorId};
        }
    }
    return SectorEditorConfigTarget{};
}

bool ApplySectorEditorSectorConfig(
        SectorAuthoringFaceAnchor& destination,
        const SectorAuthoringFaceAnchor& source)
{
    SectorAuthoringFaceAnchor candidate = source;
    candidate.id = destination.id;
    candidate.name = destination.name;
    candidate.x = destination.x;
    candidate.y = destination.y;
    if (SameSectorConfig(destination, candidate)) {
        return false;
    }
    destination = std::move(candidate);
    return true;
}

SectorEditorConfigClipboardService::SectorEditorConfigClipboardService(
        SectorEditorConfigClipboardServiceContext context)
    : context_(std::move(context))
{
}

SectorEditorConfigTarget SectorEditorConfigClipboardService::CurrentTarget() const
{
    return ResolveSectorEditorConfigTarget(
            context_.editorState.mode,
            context_.map,
            context_.authoringGraph,
            context_.derivation,
            context_.selectionState,
            context_.previewSelectionState);
}

bool SectorEditorConfigClipboardService::CanCopy() const
{
    return CurrentTarget().kind != SectorEditorConfigKind::None;
}

bool SectorEditorConfigClipboardService::CanPaste() const
{
    const SectorEditorConfigTarget target = CurrentTarget();
    return target.kind != SectorEditorConfigKind::None
            && target.kind == context_.clipboard.kind;
}

bool SectorEditorConfigClipboardService::Copy()
{
    const SectorEditorConfigTarget target = CurrentTarget();
    if (target.kind == SectorEditorConfigKind::None) {
        context_.statusText = "Select a supported map primitive first.";
        return false;
    }
    if (target.kind == SectorEditorConfigKind::Sector) {
        const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(
                context_.authoringGraph,
                target.id);
        if (anchor == nullptr) return false;
        context_.clipboard.kind = target.kind;
        context_.clipboard.payload = *anchor;
        context_.statusText = "Copied sector config.";
        return true;
    }
    if (IsRuntimeObjectKind(target.kind)) {
        return context_.runtimeObjectEditing.CopySelectedConfig(context_.clipboard);
    }
    if (IsLightKind(target.kind)) {
        return context_.lightEditing.CopySelectedConfig(context_.clipboard);
    }
    if (IsSurfaceKind(target.kind)) {
        TopologyMaterialPayload payload;
        if (!context_.materialEditing.CopyMaterial(target.surface, payload)) {
            return false;
        }
        context_.clipboard.kind = target.kind;
        context_.clipboard.payload = std::move(payload);
        return true;
    }
    return false;
}

bool SectorEditorConfigClipboardService::Paste()
{
    const SectorEditorConfigTarget target = CurrentTarget();
    if (target.kind == SectorEditorConfigKind::None) {
        context_.statusText = "Select a supported map primitive first.";
        return false;
    }
    if (target.kind != context_.clipboard.kind) {
        context_.statusText = "Copied config does not match the selected primitive type.";
        return false;
    }
    if (target.kind == SectorEditorConfigKind::Sector) {
        const auto* source = std::get_if<SectorAuthoringFaceAnchor>(
                &context_.clipboard.payload);
        SectorAuthoringFaceAnchor* destination = FindSectorAuthoringFaceAnchor(
                context_.authoringGraph,
                target.id);
        if (source == nullptr || destination == nullptr) return false;
        SectorAuthoringFaceAnchor candidate = *destination;
        if (!ApplySectorEditorSectorConfig(candidate, *source)) {
            context_.statusText = "Selected sector already matches copied config.";
            return false;
        }
        const bool changed = MutateSectorEditorAuthoringFaceAnchorById(
                context_.editorState,
                context_.lifecycle,
                context_.map,
                context_.authoringGraph,
                context_.derivation,
                target.id,
                "Pasted sector config",
                [candidate](SectorAuthoringFaceAnchor& anchor) {
                    anchor = candidate;
                    return true;
                });
        if (changed) {
            context_.uiState.floorInput = {};
            context_.uiState.ceilingInput = {};
            context_.uiState.ambientIntensityInput = {};
            context_.uiState.ambientRedInput = {};
            context_.uiState.ambientGreenInput = {};
            context_.uiState.ambientBlueInput = {};
            context_.uiState.roomtoneBufferedFaceId = -1;
        }
        return changed;
    }
    if (IsRuntimeObjectKind(target.kind)) {
        return context_.runtimeObjectEditing.PasteSelectedConfig(context_.clipboard);
    }
    if (IsLightKind(target.kind)) {
        return context_.lightEditing.PasteSelectedConfig(context_.clipboard).changed;
    }
    if (IsSurfaceKind(target.kind)) {
        const auto* payload = std::get_if<TopologyMaterialPayload>(
                &context_.clipboard.payload);
        return payload != nullptr
                && context_.materialEditing.PasteMaterial(
                        target.surface,
                        *payload,
                        context_.assets);
    }
    return false;
}

} // namespace game
