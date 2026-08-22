#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

namespace game {

namespace {

bool IsSidePickerTarget(TopologyTexturePickerTargetKind kind)
{
    return kind == TopologyTexturePickerTargetKind::SideDef
            || kind == TopologyTexturePickerTargetKind::AuthoringSide;
}

bool HasCurrentAuthoringDerivation(
        SectorEditorConstDerivationDocumentAccess derivation,
        const SectorAuthoringGraph& authoringGraph)
{
    return HasAuthoringGraphData(authoringGraph)
            && IsSectorEditorAuthoringDerivationCurrent(derivation);
}

bool IsAuthoringFaceAnchorDecalTextureField(TopologySectorTextureField field)
{
    return field == TopologySectorTextureField::Floor
            || field == TopologySectorTextureField::Ceiling
            || field == TopologySectorTextureField::DefaultWall
            || field == TopologySectorTextureField::DefaultLower
            || field == TopologySectorTextureField::DefaultUpper;
}

bool ResolveAuthoringFaceAnchorPickerTarget(
        const SectorTopologyMap& topologyMap,
        SectorEditorConstDerivationDocumentAccess derivation,
        const SectorAuthoringGraph& authoringGraph,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)) {
        outStatus = "Authoring face texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorTopologySector(topologyMap, picker.topologySectorId) == nullptr) {
        outStatus = "Authoring face texture edit unavailable: selected sector is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSectorMapping& mapping : derivation.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != picker.topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(authoringGraph, mapping.faceAnchorId) == nullptr) {
            continue;
        }
        ++matchCount;
    }

    if (matchCount == 0) {
        outStatus = "Authoring face texture edit unavailable: selected sector has no face anchor mapping";
        return false;
    }
    if (matchCount > 1) {
        outStatus = "Authoring face texture edit unavailable: selected sector has ambiguous face anchor mapping";
        return false;
    }
    return true;
}

bool ResolveDirectAuthoringFaceAnchorPickerTarget(
        SectorEditorConstDerivationDocumentAccess derivation,
        const SectorAuthoringGraph& authoringGraph,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)) {
        outStatus = "Authoring face texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorAuthoringFaceAnchor(authoringGraph, picker.authoringFaceAnchorId) == nullptr) {
        outStatus = "Authoring face texture edit unavailable: selected face anchor is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSectorMapping& mapping : derivation.authoringDerivation.mapping.sectors) {
        if (mapping.faceAnchorId == picker.authoringFaceAnchorId) {
            ++matchCount;
        }
    }
    if (matchCount == 0) {
        outStatus = "Authoring face texture edit unavailable: selected face anchor has no current derived mapping";
        return false;
    }
    if (matchCount > 1) {
        outStatus = "Authoring face texture edit unavailable: selected face anchor mapping is ambiguous";
        return false;
    }
    return true;
}

bool ResolveAuthoringSidePickerTarget(
        const SectorTopologyMap& topologyMap,
        SectorEditorConstDerivationDocumentAccess derivation,
        const SectorAuthoringGraph& authoringGraph,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)) {
        outStatus = "Authoring side texture edit unavailable: derived topology is not current";
        return false;
    }

    const SectorTopologySideDef* sideDef =
            FindSectorTopologySideDef(topologyMap, picker.topologySideDefId);
    if (sideDef == nullptr) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef is not current";
        return false;
    }
    if (picker.topologyWallPart == TopologyWallPart::Middle
            && !IsTopologyMiddleEligible(topologyMap, sideDef)) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef cannot use a middle texture";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSideMapping& mapping : derivation.authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId != picker.topologySideDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(authoringGraph, mapping.authoringLineId) == nullptr) {
            continue;
        }
        ++matchCount;
    }

    if (matchCount == 0) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef has no authoring side mapping";
        return false;
    }
    if (matchCount > 1) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef has ambiguous authoring side mapping";
        return false;
    }
    return true;
}

bool ResolveDirectAuthoringSidePickerTarget(
        SectorEditorConstDerivationDocumentAccess derivation,
        const SectorAuthoringGraph& authoringGraph,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)) {
        outStatus = "Authoring side texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorAuthoringLine(authoringGraph, picker.authoringLineId) == nullptr) {
        outStatus = "Authoring side texture edit unavailable: selected authoring line is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSideMapping& mapping : derivation.authoringDerivation.mapping.sides) {
        if (mapping.authoringLineId == picker.authoringLineId
                && mapping.authoringSide == picker.authoringSide) {
            ++matchCount;
        }
    }
    if (matchCount == 0) {
        outStatus = "Authoring side texture edit unavailable: selected authoring side has no current derived mapping";
        return false;
    }
    return true;
}

void AssignDecalTexture(
        SectorTopologyDecalLayer& decal,
        const std::string& selectedTexture,
        bool& changed)
{
    if (decal.materialId.empty()) {
        ResetTopologyUv(decal.uv);
        decal.opacity = 1.0f;
        decal.emissive = false;
        decal.tint = Vector3{1.0f, 1.0f, 1.0f};
        decal.bloomIntensity = 1.0f;
    }
    if (decal.materialId != selectedTexture) {
        decal.materialId = selectedTexture;
        changed = true;
    }
}

void AssignTexture(std::string& materialId, const std::string& selectedTexture, bool& changed)
{
    if (materialId != selectedTexture) {
        materialId = selectedTexture;
        changed = true;
    }
}

bool AssignSelectedTextureToAuthoringFaceAnchor(
        SectorAuthoringFaceAnchor& anchor,
        const TexturePickerState& picker,
        const std::string& selectedTexture,
        SectorEditorTexturePickerApplyResult& result)
{
    bool changed = false;
    switch (picker.topologyField) {
        case TopologySectorTextureField::Floor:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.floorDecal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.floorMaterialId, selectedTexture, changed);
            }
            result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? "Selected floor decal texture."
                    : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::Ceiling:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.ceilingDecal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.ceilingMaterialId, selectedTexture, changed);
            }
            result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? "Selected ceiling decal texture."
                    : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultWall:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultWall.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultWall.materialId, selectedTexture, changed);
            }
            result.status = TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultLower:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultLower.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultLower.materialId, selectedTexture, changed);
            }
            result.status = TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultUpper:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultUpper.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultUpper.materialId, selectedTexture, changed);
            }
            result.status = TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::None:
            return false;
    }
    result.changed = changed;
    return changed;
}

bool AssignSelectedTextureToAuthoringSide(
        SectorAuthoringLineSide& side,
        const TexturePickerState& picker,
        const std::string& selectedTexture,
        SectorEditorTexturePickerApplyResult& result)
{
    SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(side, picker.topologyWallPart);
    const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
            ? TopologyMaterialLayer::Base
            : picker.topologyLayer;
    bool changed = false;
    if (layer == TopologyMaterialLayer::Decal) {
        AssignDecalTexture(part.decal, selectedTexture, changed);
        result.status = TextFormat(
                "Selected %s decal texture.",
                TopologyWallPartStatusName(picker.topologyWallPart));
    } else {
        AssignTexture(part.materialId, selectedTexture, changed);
        result.status = picker.topologyWallPart == TopologyWallPart::Middle
                ? "Selected middle texture."
                : TextFormat(
                        "Changed topology sidedef %d %s texture",
                        picker.topologySideDefId,
                        TopologyWallPartStatusName(picker.topologyWallPart));
    }
    result.changed = changed;
    result.useMaterialMutationFinish = picker.topologyWallPart == TopologyWallPart::Middle;
    return changed;
}

SectorSurfaceRef BuildFlatSurface(const TexturePickerState& picker)
{
    SectorSurfaceRef surface;
    surface.kind = picker.topologyField == TopologySectorTextureField::Floor
            ? SectorSurfaceKind::Floor
            : SectorSurfaceKind::Ceiling;
    surface.topologySectorId = picker.topologySectorId;
    return surface;
}

void ApplyDirectAuthoringPreviewRebuildIfNeeded(
        const SectorEditorTexturePickerApplyResult& result,
        SectorEditorMaterialPickerRoutingContext* context)
{
    if (context != nullptr
            && result.changed
            && result.rebuildPreviewOnApply
            && context->rebuildPreviewForTexturePickerApply) {
        context->rebuildPreviewForTexturePickerApply();
    }
}

} // namespace

bool IsSectorEditorMaterialTexturePickerTarget(TopologyTexturePickerTargetKind kind)
{
    return kind == TopologyTexturePickerTargetKind::Sector
            || kind == TopologyTexturePickerTargetKind::SideDef
            || kind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor
            || kind == TopologyTexturePickerTargetKind::AuthoringSide;
}

std::string CurrentSectorEditorMaterialPickerTexture(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        const TexturePickerState& picker)
{
    if (IsSidePickerTarget(picker.topologyTargetKind)) {
        if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
            SectorAuthoringSideId sideId{picker.authoringLineId, picker.authoringSide};
            if (!IsValidSectorAuthoringId(sideId.lineId)) {
                FindSectorEditorAuthoringSideIdForTopologySideDef(
                        authoringGraph,
                        derivation.authoringDerivation,
                        picker.topologySideDefId,
                        sideId);
            }
            if (IsValidSectorAuthoringId(sideId.lineId)) {
                const SectorAuthoringLineSide* authoringSide =
                        FindSectorAuthoringLineSide(authoringGraph, sideId);
                if (authoringSide != nullptr) {
                    const SectorTopologyWallPartSettings& part =
                            TopologyWallPartSettingsFor(*authoringSide, picker.topologyWallPart);
                    const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                            ? TopologyMaterialLayer::Base
                            : picker.topologyLayer;
                    return layer == TopologyMaterialLayer::Decal
                            ? part.decal.materialId
                            : part.materialId;
                }
            }
        } else {
            const SectorTopologySideDef* sideDef =
                    FindSectorTopologySideDef(topologyMap, picker.topologySideDefId);
            if (sideDef != nullptr) {
                const SectorTopologyWallPartSettings& part =
                        TopologyWallPartSettingsFor(*sideDef, picker.topologyWallPart);
                const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                        ? TopologyMaterialLayer::Base
                        : picker.topologyLayer;
                return layer == TopologyMaterialLayer::Decal
                        ? part.decal.materialId
                        : part.materialId;
            }
        }
        return std::string{};
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor) {
        int faceAnchorId = picker.authoringFaceAnchorId;
        if (!IsValidSectorAuthoringId(faceAnchorId)) {
            faceAnchorId = FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    authoringGraph,
                    derivation.authoringDerivation,
                    picker.topologySectorId);
        }
        const SectorAuthoringFaceAnchor* anchor =
                FindSectorAuthoringFaceAnchor(authoringGraph, faceAnchorId);
        if (anchor == nullptr) {
            return std::string{};
        }
        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->floorDecal.materialId
                        : anchor->floorMaterialId;
            case TopologySectorTextureField::Ceiling:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->ceilingDecal.materialId
                        : anchor->ceilingMaterialId;
            case TopologySectorTextureField::DefaultWall:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultWall.decal.materialId
                        : anchor->defaultWall.materialId;
            case TopologySectorTextureField::DefaultLower:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultLower.decal.materialId
                        : anchor->defaultLower.materialId;
            case TopologySectorTextureField::DefaultUpper:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultUpper.decal.materialId
                        : anchor->defaultUpper.materialId;
            case TopologySectorTextureField::None:
                break;
        }
        return std::string{};
    }

    const SectorTopologySector* sector = FindSectorTopologySector(topologyMap, picker.topologySectorId);
    if (sector == nullptr) {
        return std::string{};
    }
    switch (picker.topologyField) {
        case TopologySectorTextureField::Floor:
            return picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->floorDecal.materialId
                    : sector->floorMaterialId;
        case TopologySectorTextureField::Ceiling:
            return picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->ceilingDecal.materialId
                    : sector->ceilingMaterialId;
        case TopologySectorTextureField::DefaultWall:
            return sector->defaultWall.materialId;
        case TopologySectorTextureField::DefaultLower:
            return sector->defaultLower.materialId;
        case TopologySectorTextureField::DefaultUpper:
            return sector->defaultUpper.materialId;
        case TopologySectorTextureField::None:
            break;
    }
    return std::string{};
}

std::string CurrentSectorEditorMaterialPickerTexture(
        const SectorTopologyMap& topologyMap,
        SectorEditorConstAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        const TexturePickerState& picker)
{
    return CurrentSectorEditorMaterialPickerTexture(topologyMap, authoring.graph, derivation, picker);
}

bool OpenSectorEditorMaterialPickerForDerivedSector(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    if (!HasAuthoringGraphData(authoringGraph)
            || !IsSectorEditorAuthoringDerivationCurrent(derivation)
            || FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    authoringGraph,
                    derivation.authoringDerivation,
                    topologySectorId) < 0
            || field == TopologySectorTextureField::None
            || (layer == TopologyMaterialLayer::Decal
                    && !IsAuthoringFaceAnchorDecalTextureField(field))) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.authoringSurface3DFlatTarget = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::AuthoringFaceAnchor;
    picker.topologyLayer = layer;
    picker.topologySectorId = topologySectorId;
    picker.topologyField = field;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;
    picker.authoringFaceAnchorId = -1;
    picker.authoringLineId = -1;
    picker.authoringSide = SectorTopologySideKind::Front;

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(topologyMap),
            CurrentSectorEditorMaterialPickerTexture(topologyMap, authoringGraph, derivation, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForDerivedSector(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForDerivedSector(
            picker,
            topologyMap,
            authoring.graph,
            derivation,
            topologySectorId,
            field,
            layer);
}

bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)
            || FindSectorAuthoringFaceAnchor(authoringGraph, faceAnchorId) == nullptr
            || field == TopologySectorTextureField::None
            || (layer == TopologyMaterialLayer::Decal
                    && !IsAuthoringFaceAnchorDecalTextureField(field))) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.authoringSurface3DFlatTarget = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::AuthoringFaceAnchor;
    picker.topologyLayer = layer;
    picker.topologySectorId = -1;
    picker.topologyField = field;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;
    picker.authoringFaceAnchorId = faceAnchorId;
    picker.authoringLineId = -1;
    picker.authoringSide = SectorTopologySideKind::Front;

    std::string status;
    if (!ResolveDirectAuthoringFaceAnchorPickerTarget(derivation, authoringGraph, picker, status)) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(topologyMap),
            CurrentSectorEditorMaterialPickerTexture(topologyMap, authoringGraph, derivation, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
            picker,
            topologyMap,
            authoring.graph,
            derivation,
            faceAnchorId,
            field,
            layer);
}

bool OpenSectorEditorMaterialPickerForDerivedSideDef(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(topologyMap, topologySideDefId);
    SectorAuthoringSideId sideId;
    if (!HasAuthoringGraphData(authoringGraph)
            || !IsSectorEditorAuthoringDerivationCurrent(derivation)
            || sideDef == nullptr
            || !FindSectorEditorAuthoringSideIdForTopologySideDef(
                    authoringGraph,
                    derivation.authoringDerivation,
                    topologySideDefId,
                    sideId)
            || (wallPart == TopologyWallPart::Middle
                    && !IsTopologyMiddleEligible(topologyMap, sideDef))) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.authoringSurface3DFlatTarget = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::AuthoringSide;
    picker.topologyLayer = wallPart == TopologyWallPart::Middle
            ? TopologyMaterialLayer::Base
            : layer;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = topologySideDefId;
    picker.topologyWallPart = wallPart;
    picker.authoringFaceAnchorId = -1;
    picker.authoringLineId = -1;
    picker.authoringSide = SectorTopologySideKind::Front;

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(topologyMap),
            CurrentSectorEditorMaterialPickerTexture(topologyMap, authoringGraph, derivation, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForDerivedSideDef(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForDerivedSideDef(
            picker,
            topologyMap,
            authoring.graph,
            derivation,
            topologySideDefId,
            wallPart,
            layer);
}

bool OpenSectorEditorMaterialPickerForAuthoringSide(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    if (!HasCurrentAuthoringDerivation(derivation, authoringGraph)
            || FindSectorAuthoringLine(authoringGraph, sideId.lineId) == nullptr) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.authoringSurface3DFlatTarget = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::AuthoringSide;
    picker.topologyLayer = wallPart == TopologyWallPart::Middle
            ? TopologyMaterialLayer::Base
            : layer;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = wallPart;
    picker.authoringFaceAnchorId = -1;
    picker.authoringLineId = sideId.lineId;
    picker.authoringSide = sideId.side;

    std::string status;
    if (!ResolveDirectAuthoringSidePickerTarget(derivation, authoringGraph, picker, status)) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(topologyMap),
            CurrentSectorEditorMaterialPickerTexture(topologyMap, authoringGraph, derivation, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForAuthoringSide(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSide(
            picker,
            topologyMap,
            authoring.graph,
            derivation,
            sideId,
            wallPart,
            layer);
}

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        TexturePickerState& texturePicker,
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation)
{
    TexturePickerState picker = texturePicker;
    if (!picker.open || !IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return SectorEditorTexturePickerApplyResult{};
    }

    SectorEditorTexturePickerApplyResult result;
    const auto closeAndReturn = [&texturePicker, &result]() {
        CloseSectorEditorTexturePicker(texturePicker);
        return result;
    };

    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        return closeAndReturn();
    }

    const std::string selectedTexture = selected.materialId;
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;
    const SectorEditorConstDerivationDocumentAccess constDerivation =
            MakeSectorEditorConstDerivationDocumentAccess(derivation);

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor) {
        if (IsValidSectorAuthoringId(picker.authoringFaceAnchorId)) {
            if (!ResolveDirectAuthoringFaceAnchorPickerTarget(
                        constDerivation,
                        authoringGraph,
                        picker,
                        result.status)) {
                return closeAndReturn();
            }

            const char* status = "Updated authoring face texture";
            if (MutateSectorEditorAuthoringFaceAnchorById(
                        lifecycle,
                        topologyRenderRevision,
                        topologyRenderCache,
                        topologyMap,
                        authoringGraph,
                        derivation,
                        picker.authoringFaceAnchorId,
                        status,
                        [picker, selectedTexture](SectorAuthoringFaceAnchor& anchor) {
                            switch (picker.topologyField) {
                                case TopologySectorTextureField::Floor:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.floorDecal.materialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.floorDecal.materialId = selectedTexture;
                                    } else {
                                        if (anchor.floorMaterialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.floorMaterialId = selectedTexture;
                                    }
                                    return true;
                                case TopologySectorTextureField::Ceiling:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.ceilingDecal.materialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.ceilingDecal.materialId = selectedTexture;
                                    } else {
                                        if (anchor.ceilingMaterialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.ceilingMaterialId = selectedTexture;
                                    }
                                    return true;
                                case TopologySectorTextureField::DefaultWall:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultWall.decal.materialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultWall.decal.materialId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultWall.materialId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultWall.materialId = selectedTexture;
                                    return true;
                                case TopologySectorTextureField::DefaultLower:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultLower.decal.materialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultLower.decal.materialId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultLower.materialId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultLower.materialId = selectedTexture;
                                    return true;
                                case TopologySectorTextureField::DefaultUpper:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultUpper.decal.materialId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultUpper.decal.materialId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultUpper.materialId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultUpper.materialId = selectedTexture;
                                    return true;
                                case TopologySectorTextureField::None:
                                    break;
                            }
                            return false;
                        })) {
                result.changed = true;
                result.status = status;
            }
            return closeAndReturn();
        }

        if (!ResolveAuthoringFaceAnchorPickerTarget(
                    topologyMap,
                    constDerivation,
                    authoringGraph,
                    picker,
                    result.status)) {
            return closeAndReturn();
        }

        if (!MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                    lifecycle,
                    topologyRenderRevision,
                    topologyRenderCache,
                    topologyMap,
                    authoringGraph,
                    derivation,
                    picker.topologySectorId,
                    "Updated authoring face anchor texture",
                    [picker, selectedTexture, &result](SectorAuthoringFaceAnchor& anchor) {
                        return AssignSelectedTextureToAuthoringFaceAnchor(
                                anchor,
                                picker,
                                selectedTexture,
                                result);
                    })) {
            result.changed = false;
        }
        return closeAndReturn();
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
        if (IsValidSectorAuthoringId(picker.authoringLineId)) {
            if (!ResolveDirectAuthoringSidePickerTarget(
                        constDerivation,
                        authoringGraph,
                        picker,
                        result.status)) {
                return closeAndReturn();
            }

            const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                    ? TopologyMaterialLayer::Base
                    : picker.topologyLayer;
            const char* status = "Updated authoring side texture";
            if (MutateSectorEditorAuthoringSideById(
                        lifecycle,
                        topologyRenderRevision,
                        topologyRenderCache,
                        topologyMap,
                        authoringGraph,
                        derivation,
                        SectorAuthoringSideId{picker.authoringLineId, picker.authoringSide},
                        status,
                        [picker, layer, selectedTexture](SectorAuthoringLineSide& side) {
                            SectorTopologyWallPartSettings& part =
                                    TopologyWallPartSettingsFor(side, picker.topologyWallPart);
                            std::string& target = layer == TopologyMaterialLayer::Decal
                                    ? part.decal.materialId
                                    : part.materialId;
                            if (target == selectedTexture) {
                                return false;
                            }
                            target = selectedTexture;
                            return true;
                        })) {
                result.changed = true;
                result.status = status;
            }
            return closeAndReturn();
        }

        if (!ResolveAuthoringSidePickerTarget(
                    topologyMap,
                    constDerivation,
                    authoringGraph,
                    picker,
                    result.status)) {
            return closeAndReturn();
        }

        if (!MutateSectorEditorAuthoringSideForTopologySideDef(
                    lifecycle,
                    topologyRenderRevision,
                    topologyRenderCache,
                    topologyMap,
                    authoringGraph,
                    derivation,
                    picker.topologySideDefId,
                    "Updated authoring side texture",
                    [picker, selectedTexture, &result](SectorAuthoringLineSide& side) {
                        return AssignSelectedTextureToAuthoringSide(
                                side,
                                picker,
                                selectedTexture,
                                result);
                    })) {
            result.changed = false;
        }
        return closeAndReturn();
    }

    result.status = "Cannot edit material: authoring data is required.";
    return closeAndReturn();
}

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        TexturePickerState& texturePicker,
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorDerivationDocumentAccess derivation)
{
    return ApplySectorEditorMaterialTexturePickerSelection(
            texturePicker,
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            topologyMap,
            authoring.graph,
            derivation);
}

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        SectorEditorMaterialPickerRoutingContext& context,
        engine::AssetManager* assets)
{
    (void)assets;
    TexturePickerState picker = context.texturePicker;
    if (!picker.open || !IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return SectorEditorTexturePickerApplyResult{};
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
        SectorEditorTexturePickerApplyResult result =
                ApplySectorEditorMaterialTexturePickerSelection(
                        context.texturePicker,
                        context.lifecycle,
                        context.topologyRenderRevision,
                        context.topologyRenderCache,
                        context.topologyMap,
                        context.authoringGraph,
                        context.derivation);
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        ApplyDirectAuthoringPreviewRebuildIfNeeded(result, &context);
        return result;
    }

    if (!HasAuthoringGraphData(context.authoringGraph)) {
        context.statusText = "Cannot edit material: authoring data is required.";
        CloseSectorEditorTexturePicker(context.texturePicker);
        return SectorEditorTexturePickerApplyResult{};
    }

    const bool routeAuthoringSideMaterial =
            HasAuthoringGraphData(context.authoringGraph)
            && picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef;
    const bool routeAuthoringFlatMaterial =
            HasAuthoringGraphData(context.authoringGraph)
            && picker.authoringSurface3DFlatTarget
            && picker.topologyTargetKind == TopologyTexturePickerTargetKind::Sector
            && (picker.topologyField == TopologySectorTextureField::Floor
                    || picker.topologyField == TopologySectorTextureField::Ceiling);
    if (routeAuthoringSideMaterial || routeAuthoringFlatMaterial) {
        if (!IsSectorEditorAuthoringDerivationCurrent(context.derivation)) {
            context.statusText = routeAuthoringSideMaterial
                    ? "Wall material edit unavailable: derived topology is not current"
                    : "3D surface edit unavailable: derived topology is not current";
            CloseSectorEditorTexturePicker(context.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (routeAuthoringSideMaterial) {
        SectorAuthoringSideId sideId;
        if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                    context.authoringGraph,
                    context.derivation.authoringDerivation,
                    picker.topologySideDefId,
                    sideId)) {
            context.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
            CloseSectorEditorTexturePicker(context.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
        const SectorTopologySideDef* sideDef =
                FindSectorTopologySideDef(context.topologyMap, picker.topologySideDefId);
        if (sideDef == nullptr) {
            context.statusText = "Wall material edit unavailable: selected sidedef is not current";
            CloseSectorEditorTexturePicker(context.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (routeAuthoringFlatMaterial) {
        SectorEditorAuthoringSurfaceTarget surfaceTarget;
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    context.topologyMap,
                    context.authoringGraph,
                    context.derivation.authoringDerivation,
                    IsSectorEditorAuthoringDerivationCurrent(context.derivation),
                    BuildFlatSurface(picker),
                    surfaceTarget,
                    &unavailableStatus)
                || surfaceTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
            context.statusText = unavailableStatus.empty()
                    ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                    : unavailableStatus;
            CloseSectorEditorTexturePicker(context.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (!routeAuthoringSideMaterial && !routeAuthoringFlatMaterial) {
        context.statusText = "Cannot edit material: selected derived target has no authoring material route.";
        CloseSectorEditorTexturePicker(context.texturePicker);
        return SectorEditorTexturePickerApplyResult{};
    }

    SectorEditorTexturePickerApplyResult result;
    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        CloseSectorEditorTexturePicker(context.texturePicker);
        return result;
    }
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;
    CloseSectorEditorTexturePicker(context.texturePicker);
    if (routeAuthoringSideMaterial) {
        const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
                context.lifecycle,
                context.topologyRenderRevision,
                context.topologyRenderCache,
                context.topologyMap,
                context.authoringGraph,
                context.derivation,
                picker.topologySideDefId,
                "Updated authoring side texture",
                [&](SectorAuthoringLineSide& side) {
                    return AssignSelectedTextureToAuthoringSide(side, picker, selected.materialId, result);
                });
        if (!refreshed && result.changed) {
            result.changed = false;
            context.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
            return result;
        }
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        if (result.changed && result.rebuildPreviewOnApply && context.rebuildPreviewForTexturePickerApply) {
            context.rebuildPreviewForTexturePickerApply();
        }
        return result;
    }

    if (routeAuthoringFlatMaterial) {
        const bool refreshed = MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                context.lifecycle,
                context.topologyRenderRevision,
                context.topologyRenderCache,
                context.topologyMap,
                context.authoringGraph,
                context.derivation,
                picker.topologySectorId,
                "Updated authoring face anchor texture",
                [&](SectorAuthoringFaceAnchor& anchor) {
                    return AssignSelectedTextureToAuthoringFaceAnchor(anchor, picker, selected.materialId, result);
                });
        if (!refreshed && result.changed) {
            result.changed = false;
            context.statusText = "3D flat surface edit unavailable: selected sector has no face anchor mapping";
            return result;
        }
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        if (result.changed && result.rebuildPreviewOnApply && context.rebuildPreviewForTexturePickerApply) {
            context.rebuildPreviewForTexturePickerApply();
        }
        return result;
    }

    return result;
}

} // namespace game
