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

bool HasCurrentAuthoringDerivation(const SectorEditorState& state)
{
    return HasAuthoringGraphData(state)
            && state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !state.authoringDerivedTopologyStale
            && state.authoringDerivation.success;
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
        const SectorEditorState& state,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(state)) {
        outStatus = "Authoring face texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorTopologySector(state.topologyMap, picker.topologySectorId) == nullptr) {
        outStatus = "Authoring face texture edit unavailable: selected sector is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSectorMapping& mapping : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != picker.topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(state.authoringGraph, mapping.faceAnchorId) == nullptr) {
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
        const SectorEditorState& state,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(state)) {
        outStatus = "Authoring face texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorAuthoringFaceAnchor(state.authoringGraph, picker.authoringFaceAnchorId) == nullptr) {
        outStatus = "Authoring face texture edit unavailable: selected face anchor is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSectorMapping& mapping : state.authoringDerivation.mapping.sectors) {
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
        const SectorEditorState& state,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(state)) {
        outStatus = "Authoring side texture edit unavailable: derived topology is not current";
        return false;
    }

    const SectorTopologySideDef* sideDef =
            FindSectorTopologySideDef(state.topologyMap, picker.topologySideDefId);
    if (sideDef == nullptr) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef is not current";
        return false;
    }
    if (picker.topologyWallPart == TopologyWallPart::Middle
            && !IsTopologyMiddleEligible(state.topologyMap, sideDef)) {
        outStatus = "Authoring side texture edit unavailable: selected sidedef cannot use a middle texture";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSideMapping& mapping : state.authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId != picker.topologySideDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(state.authoringGraph, mapping.authoringLineId) == nullptr) {
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
        const SectorEditorState& state,
        const TexturePickerState& picker,
        std::string& outStatus)
{
    outStatus.clear();

    if (!HasCurrentAuthoringDerivation(state)) {
        outStatus = "Authoring side texture edit unavailable: derived topology is not current";
        return false;
    }
    if (FindSectorAuthoringLine(state.authoringGraph, picker.authoringLineId) == nullptr) {
        outStatus = "Authoring side texture edit unavailable: selected authoring line is not current";
        return false;
    }

    int matchCount = 0;
    for (const SectorAuthoringDerivedSideMapping& mapping : state.authoringDerivation.mapping.sides) {
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
    if (decal.textureId.empty()) {
        ResetTopologyUv(decal.uv);
        decal.opacity = 1.0f;
        decal.emissive = false;
        decal.tint = Vector3{1.0f, 1.0f, 1.0f};
        decal.bloomIntensity = 1.0f;
    }
    if (decal.textureId != selectedTexture) {
        decal.textureId = selectedTexture;
        changed = true;
    }
}

void AssignTexture(std::string& textureId, const std::string& selectedTexture, bool& changed)
{
    if (textureId != selectedTexture) {
        textureId = selectedTexture;
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
                AssignTexture(anchor.floorTextureId, selectedTexture, changed);
            }
            result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? "Selected floor decal texture."
                    : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::Ceiling:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.ceilingDecal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.ceilingTextureId, selectedTexture, changed);
            }
            result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? "Selected ceiling decal texture."
                    : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultWall:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultWall.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultWall.textureId, selectedTexture, changed);
            }
            result.status = TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultLower:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultLower.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultLower.textureId, selectedTexture, changed);
            }
            result.status = TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
            break;
        case TopologySectorTextureField::DefaultUpper:
            if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                AssignDecalTexture(anchor.defaultUpper.decal, selectedTexture, changed);
            } else {
                AssignTexture(anchor.defaultUpper.textureId, selectedTexture, changed);
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
        AssignTexture(part.textureId, selectedTexture, changed);
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
        const SectorEditorState& state,
        const TexturePickerState& picker)
{
    if (IsSidePickerTarget(picker.topologyTargetKind)) {
        if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
            SectorAuthoringSideId sideId{picker.authoringLineId, picker.authoringSide};
            if (!IsValidSectorAuthoringId(sideId.lineId)) {
                FindSectorEditorAuthoringSideIdForTopologySideDef(
                        state,
                        picker.topologySideDefId,
                        sideId);
            }
            if (IsValidSectorAuthoringId(sideId.lineId)) {
                const SectorAuthoringLineSide* authoringSide =
                        FindSectorAuthoringLineSide(state.authoringGraph, sideId);
                if (authoringSide != nullptr) {
                    const SectorTopologyWallPartSettings& part =
                            TopologyWallPartSettingsFor(*authoringSide, picker.topologyWallPart);
                    const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                            ? TopologyMaterialLayer::Base
                            : picker.topologyLayer;
                    return layer == TopologyMaterialLayer::Decal
                            ? part.decal.textureId
                            : part.textureId;
                }
            }
        } else {
            const SectorTopologySideDef* sideDef =
                    FindSectorTopologySideDef(state.topologyMap, picker.topologySideDefId);
            if (sideDef != nullptr) {
                const SectorTopologyWallPartSettings& part =
                        TopologyWallPartSettingsFor(*sideDef, picker.topologyWallPart);
                const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                        ? TopologyMaterialLayer::Base
                        : picker.topologyLayer;
                return layer == TopologyMaterialLayer::Decal
                        ? part.decal.textureId
                        : part.textureId;
            }
        }
        return std::string{};
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor) {
        int faceAnchorId = picker.authoringFaceAnchorId;
        if (!IsValidSectorAuthoringId(faceAnchorId)) {
            faceAnchorId = FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    state,
                    picker.topologySectorId);
        }
        const SectorAuthoringFaceAnchor* anchor =
                FindSectorAuthoringFaceAnchor(state.authoringGraph, faceAnchorId);
        if (anchor == nullptr) {
            return std::string{};
        }
        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->floorDecal.textureId
                        : anchor->floorTextureId;
            case TopologySectorTextureField::Ceiling:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->ceilingDecal.textureId
                        : anchor->ceilingTextureId;
            case TopologySectorTextureField::DefaultWall:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultWall.decal.textureId
                        : anchor->defaultWall.textureId;
            case TopologySectorTextureField::DefaultLower:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultLower.decal.textureId
                        : anchor->defaultLower.textureId;
            case TopologySectorTextureField::DefaultUpper:
                return picker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultUpper.decal.textureId
                        : anchor->defaultUpper.textureId;
            case TopologySectorTextureField::None:
                break;
        }
        return std::string{};
    }

    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, picker.topologySectorId);
    if (sector == nullptr) {
        return std::string{};
    }
    switch (picker.topologyField) {
        case TopologySectorTextureField::Floor:
            return picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->floorDecal.textureId
                    : sector->floorTextureId;
        case TopologySectorTextureField::Ceiling:
            return picker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->ceilingDecal.textureId
                    : sector->ceilingTextureId;
        case TopologySectorTextureField::DefaultWall:
            return sector->defaultWall.textureId;
        case TopologySectorTextureField::DefaultLower:
            return sector->defaultLower.textureId;
        case TopologySectorTextureField::DefaultUpper:
            return sector->defaultUpper.textureId;
        case TopologySectorTextureField::None:
            break;
    }
    return std::string{};
}

bool OpenSectorEditorMaterialPickerForDerivedSector(
        SectorEditorState& state,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (!HasAuthoringGraphData(state)
            || state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || state.authoringDerivedTopologyStale
            || !state.authoringDerivation.success
            || FindSectorEditorAuthoringFaceAnchorIdForTopologySector(state, topologySectorId) < 0
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
            SortedSectorTopologyTextureIds(state.topologyMap),
            CurrentSectorEditorMaterialPickerTexture(state, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        SectorEditorState& state,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (!HasCurrentAuthoringDerivation(state)
            || FindSectorAuthoringFaceAnchor(state.authoringGraph, faceAnchorId) == nullptr
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
    if (!ResolveDirectAuthoringFaceAnchorPickerTarget(state, picker, status)) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(state.topologyMap),
            CurrentSectorEditorMaterialPickerTexture(state, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForDerivedSideDef(
        SectorEditorState& state,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, topologySideDefId);
    SectorAuthoringSideId sideId;
    if (!HasAuthoringGraphData(state)
            || state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || state.authoringDerivedTopologyStale
            || !state.authoringDerivation.success
            || sideDef == nullptr
            || !FindSectorEditorAuthoringSideIdForTopologySideDef(state, topologySideDefId, sideId)
            || (wallPart == TopologyWallPart::Middle
                    && !IsTopologyMiddleEligible(state.topologyMap, sideDef))) {
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
            SortedSectorTopologyTextureIds(state.topologyMap),
            CurrentSectorEditorMaterialPickerTexture(state, picker));
    return true;
}

bool OpenSectorEditorMaterialPickerForAuthoringSide(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (!HasCurrentAuthoringDerivation(state)
            || FindSectorAuthoringLine(state.authoringGraph, sideId.lineId) == nullptr) {
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
    if (!ResolveDirectAuthoringSidePickerTarget(state, picker, status)) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    OpenSectorEditorTexturePicker(
            picker,
            SortedSectorTopologyTextureIds(state.topologyMap),
            CurrentSectorEditorMaterialPickerTexture(state, picker));
    return true;
}

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(SectorEditorState& state)
{
    TexturePickerState picker = state.texturePicker;
    if (!picker.open || !IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return SectorEditorTexturePickerApplyResult{};
    }

    SectorEditorTexturePickerApplyResult result;
    const auto closeAndReturn = [&state, &result]() {
        CloseSectorEditorTexturePicker(state.texturePicker);
        return result;
    };

    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        return closeAndReturn();
    }

    const std::string selectedTexture = selected.textureId;
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor) {
        if (IsValidSectorAuthoringId(picker.authoringFaceAnchorId)) {
            if (!ResolveDirectAuthoringFaceAnchorPickerTarget(state, picker, result.status)) {
                return closeAndReturn();
            }

            const char* status = "Updated authoring face texture";
            if (MutateSectorEditorAuthoringFaceAnchorById(
                        state,
                        picker.authoringFaceAnchorId,
                        status,
                        [picker, selectedTexture](SectorAuthoringFaceAnchor& anchor) {
                            switch (picker.topologyField) {
                                case TopologySectorTextureField::Floor:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.floorDecal.textureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.floorDecal.textureId = selectedTexture;
                                    } else {
                                        if (anchor.floorTextureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.floorTextureId = selectedTexture;
                                    }
                                    return true;
                                case TopologySectorTextureField::Ceiling:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.ceilingDecal.textureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.ceilingDecal.textureId = selectedTexture;
                                    } else {
                                        if (anchor.ceilingTextureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.ceilingTextureId = selectedTexture;
                                    }
                                    return true;
                                case TopologySectorTextureField::DefaultWall:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultWall.decal.textureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultWall.decal.textureId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultWall.textureId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultWall.textureId = selectedTexture;
                                    return true;
                                case TopologySectorTextureField::DefaultLower:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultLower.decal.textureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultLower.decal.textureId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultLower.textureId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultLower.textureId = selectedTexture;
                                    return true;
                                case TopologySectorTextureField::DefaultUpper:
                                    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                        if (anchor.defaultUpper.decal.textureId == selectedTexture) {
                                            return false;
                                        }
                                        anchor.defaultUpper.decal.textureId = selectedTexture;
                                        return true;
                                    }
                                    if (anchor.defaultUpper.textureId == selectedTexture) {
                                        return false;
                                    }
                                    anchor.defaultUpper.textureId = selectedTexture;
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

        if (!ResolveAuthoringFaceAnchorPickerTarget(state, picker, result.status)) {
            return closeAndReturn();
        }

        if (!MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                    state,
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
            if (!ResolveDirectAuthoringSidePickerTarget(state, picker, result.status)) {
                return closeAndReturn();
            }

            const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                    ? TopologyMaterialLayer::Base
                    : picker.topologyLayer;
            const char* status = "Updated authoring side texture";
            if (MutateSectorEditorAuthoringSideById(
                        state,
                        SectorAuthoringSideId{picker.authoringLineId, picker.authoringSide},
                        status,
                        [picker, layer, selectedTexture](SectorAuthoringLineSide& side) {
                            SectorTopologyWallPartSettings& part =
                                    TopologyWallPartSettingsFor(side, picker.topologyWallPart);
                            std::string& target = layer == TopologyMaterialLayer::Decal
                                    ? part.decal.textureId
                                    : part.textureId;
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

        if (!ResolveAuthoringSidePickerTarget(state, picker, result.status)) {
            return closeAndReturn();
        }

        if (!MutateSectorEditorAuthoringSideForTopologySideDef(
                    state,
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
        SectorEditorMaterialPickerRoutingContext& context,
        engine::AssetManager* assets)
{
    (void)assets;
    SectorEditorState& state = context.state;
    TexturePickerState picker = state.texturePicker;
    if (!picker.open || !IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return SectorEditorTexturePickerApplyResult{};
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
        SectorEditorTexturePickerApplyResult result =
                ApplySectorEditorMaterialTexturePickerSelection(state);
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        ApplyDirectAuthoringPreviewRebuildIfNeeded(result, &context);
        return result;
    }

    if (!HasAuthoringGraphData(state)) {
        context.statusText = "Cannot edit material: authoring data is required.";
        CloseSectorEditorTexturePicker(state.texturePicker);
        return SectorEditorTexturePickerApplyResult{};
    }

    const bool routeAuthoringSideMaterial =
            HasAuthoringGraphData(state)
            && picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef;
    const bool routeAuthoringFlatMaterial =
            HasAuthoringGraphData(state)
            && picker.authoringSurface3DFlatTarget
            && picker.topologyTargetKind == TopologyTexturePickerTargetKind::Sector
            && (picker.topologyField == TopologySectorTextureField::Floor
                    || picker.topologyField == TopologySectorTextureField::Ceiling);
    if (routeAuthoringSideMaterial || routeAuthoringFlatMaterial) {
        if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                || state.authoringDerivedTopologyStale
                || !state.authoringDerivation.success) {
            context.statusText = routeAuthoringSideMaterial
                    ? "Wall material edit unavailable: derived topology is not current"
                    : "3D surface edit unavailable: derived topology is not current";
            CloseSectorEditorTexturePicker(state.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (routeAuthoringSideMaterial) {
        SectorAuthoringSideId sideId;
        if (!FindSectorEditorAuthoringSideIdForTopologySideDef(state, picker.topologySideDefId, sideId)) {
            context.statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
            CloseSectorEditorTexturePicker(state.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
        const SectorTopologySideDef* sideDef =
                FindSectorTopologySideDef(state.topologyMap, picker.topologySideDefId);
        if (sideDef == nullptr) {
            context.statusText = "Wall material edit unavailable: selected sidedef is not current";
            CloseSectorEditorTexturePicker(state.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (routeAuthoringFlatMaterial) {
        SectorEditorAuthoringSurfaceTarget surfaceTarget;
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    state,
                    BuildFlatSurface(picker),
                    surfaceTarget,
                    &unavailableStatus)
                || surfaceTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
            context.statusText = unavailableStatus.empty()
                    ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                    : unavailableStatus;
            CloseSectorEditorTexturePicker(state.texturePicker);
            return SectorEditorTexturePickerApplyResult{};
        }
    }

    if (!routeAuthoringSideMaterial && !routeAuthoringFlatMaterial) {
        context.statusText = "Cannot edit material: selected derived target has no authoring material route.";
        CloseSectorEditorTexturePicker(state.texturePicker);
        return SectorEditorTexturePickerApplyResult{};
    }

    SectorEditorTexturePickerApplyResult result;
    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        CloseSectorEditorTexturePicker(state.texturePicker);
        return result;
    }
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;
    CloseSectorEditorTexturePicker(state.texturePicker);
    if (routeAuthoringSideMaterial) {
        const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
                state,
                picker.topologySideDefId,
                "Updated authoring side texture",
                [&](SectorAuthoringLineSide& side) {
                    return AssignSelectedTextureToAuthoringSide(side, picker, selected.textureId, result);
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
                state,
                picker.topologySectorId,
                "Updated authoring face anchor texture",
                [&](SectorAuthoringFaceAnchor& anchor) {
                    return AssignSelectedTextureToAuthoringFaceAnchor(anchor, picker, selected.textureId, result);
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
