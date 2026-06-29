#include "sector_editor/SectorEditorTextureModals.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <cstdio>

namespace game {

namespace {

void PopulateTexturePickerOptions(TexturePickerState& picker, const SectorTopologyMap& map, const std::string& currentTexture)
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(map);
    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());

    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

bool IsSectorPickerTarget(TopologyTexturePickerTargetKind kind)
{
    return kind == TopologyTexturePickerTargetKind::Sector
            || kind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor;
}

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
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != picker.topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(
                        state.authoringGraph,
                        mapping.faceAnchorId) == nullptr) {
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
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : state.authoringDerivation.mapping.sectors) {
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

bool IsAuthoringFaceAnchorDecalTextureField(TopologySectorTextureField field)
{
    return field == TopologySectorTextureField::Floor
            || field == TopologySectorTextureField::Ceiling
            || field == TopologySectorTextureField::DefaultWall
            || field == TopologySectorTextureField::DefaultLower
            || field == TopologySectorTextureField::DefaultUpper;
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
    for (const SectorAuthoringDerivedSideMapping& mapping
            : state.authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId != picker.topologySideDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(
                        state.authoringGraph,
                        mapping.authoringLineId) == nullptr) {
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
    for (const SectorAuthoringDerivedSideMapping& mapping
            : state.authoringDerivation.mapping.sides) {
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

void AssignSelectedTextureToPickerTarget(
        SectorTopologyMap& map,
        const TexturePickerState& picker,
        const std::string& selectedTexture,
        SectorEditorTexturePickerApplyResult& result)
{
    auto assignTexture = [&](std::string& field) {
        if (field != selectedTexture) {
            field = selectedTexture;
            result.changed = true;
        }
    };
    auto assignDecalTexture = [&](SectorTopologyDecalLayer& decal) {
        if (decal.textureId.empty()) {
            ResetTopologyUv(decal.uv);
            decal.opacity = 1.0f;
            decal.emissive = false;
            decal.tint = Vector3{1.0f, 1.0f, 1.0f};
            decal.bloomIntensity = 1.0f;
        }
        assignTexture(decal.textureId);
    };

    if (IsSectorPickerTarget(picker.topologyTargetKind)) {
        SectorTopologySector* sector = FindSectorTopologySector(map, picker.topologySectorId);
        if (sector == nullptr) {
            return;
        }

        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->floorDecal);
                } else {
                    assignTexture(sector->floorTextureId);
                }
                break;
            case TopologySectorTextureField::Ceiling:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->ceilingDecal);
                } else {
                    assignTexture(sector->ceilingTextureId);
                }
                break;
            case TopologySectorTextureField::DefaultWall:
                assignTexture(sector->defaultWall.textureId);
                break;
            case TopologySectorTextureField::DefaultLower:
                assignTexture(sector->defaultLower.textureId);
                break;
            case TopologySectorTextureField::DefaultUpper:
                assignTexture(sector->defaultUpper.textureId);
                break;
            case TopologySectorTextureField::None:
                break;
        }
        result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                ? TextFormat("Selected %s decal texture.", picker.topologyField == TopologySectorTextureField::Floor ? "floor" : "ceiling")
                : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
        return;
    }

    if (IsSidePickerTarget(picker.topologyTargetKind)) {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, picker.topologySideDefId);
        if (sideDef == nullptr) {
            return;
        }
        SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(*sideDef, picker.topologyWallPart);
        const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                ? TopologyMaterialLayer::Base
                : picker.topologyLayer;
        if (layer == TopologyMaterialLayer::Decal) {
            assignDecalTexture(part.decal);
            result.status = TextFormat(
                    "Selected %s decal texture.",
                    TopologyWallPartStatusName(picker.topologyWallPart));
        } else {
            assignTexture(part.textureId);
            result.status = picker.topologyWallPart == TopologyWallPart::Middle
                    ? "Selected middle texture."
                    : TextFormat(
                            "Changed topology sidedef %d %s texture",
                            sideDef->id,
                            TopologyWallPartStatusName(picker.topologyWallPart));
        }
        result.useMaterialMutationFinish = picker.topologyWallPart == TopologyWallPart::Middle;
    }
}

} // namespace

void RefreshAddMapTextureScan(AddMapTextureState& modalState)
{
    modalState.paths = ScanAssetImagePngs(modalState.scanMessage);
    modalState.optionLabels.clear();
    modalState.optionLabels.reserve(modalState.paths.size());
    for (const std::string& path : modalState.paths) {
        modalState.optionLabels.push_back(path.c_str());
    }
    modalState.scanned = true;
    modalState.selectedPathIndex = modalState.paths.empty() ? -1 : 0;
}

void SelectAddMapTexturePath(
        AddMapTextureState& modalState,
        const SectorTopologyMap& map,
        int pathIndex)
{
    if (pathIndex < 0 || pathIndex >= static_cast<int>(modalState.paths.size())) {
        modalState.selectedPathIndex = -1;
        modalState.textureIdBuffer[0] = '\0';
        return;
    }

    modalState.selectedPathIndex = pathIndex;
    const std::string base = GeneratedTextureIdBase(modalState.paths[static_cast<size_t>(pathIndex)]);
    std::string uniqueId = base;
    int suffix = 1;
    while (FindSectorTopologyTexture(map, uniqueId) != nullptr) {
        char suffixBuffer[16] = {};
        std::snprintf(suffixBuffer, sizeof(suffixBuffer), "_%03d", suffix);
        uniqueId = base + suffixBuffer;
        ++suffix;
    }

    std::snprintf(modalState.textureIdBuffer, sizeof(modalState.textureIdBuffer), "%s", uniqueId.c_str());
    modalState.previewPath.clear();
    modalState.previewTexture = engine::NullTextureHandle();
}

bool ValidateAddMapTextureId(const AddMapTextureState& modalState, std::string& error)
{
    error.clear();
    if (modalState.selectedPathIndex < 0 || modalState.selectedPathIndex >= static_cast<int>(modalState.paths.size())) {
        error = "Select a PNG file";
        return false;
    }

    const std::string id = modalState.textureIdBuffer;
    if (id.empty()) {
        error = "Texture ID is required";
        return false;
    }
    if (!IsValidTextureId(id)) {
        error = "Texture ID may only contain letters, digits, underscores, and dashes";
        return false;
    }
    return true;
}

SectorEditorAddTextureResult AddSelectedMapTexture(SectorEditorState& state)
{
    SectorEditorAddTextureResult result;
    if (!ValidateAddMapTextureId(state.addMapTexture, result.error)) {
        state.addMapTexture.validationMessage = result.error;
        return result;
    }

    AddMapTextureState& modalState = state.addMapTexture;
    const std::string id = modalState.textureIdBuffer;
    const std::string path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    result.replacing = FindSectorTopologyTexture(state.topologyMap, id) != nullptr;
    result.textureId = id;

    SectorTextureDefinition definition;
    definition.id = id;
    definition.path = path;
    definition.filter = modalState.filter;
    state.topologyMap.texturesById[id] = std::move(definition);

    result.success = true;
    return result;
}

std::string CurrentTextureForPickerTarget(const SectorEditorState& state)
{
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        return state.previewSettingsModal.open
                ? state.previewSettingsModal.draftSkySettings.textureId
                : state.topologyMap.skySettings.textureId;
    }

    if (IsSidePickerTarget(state.texturePicker.topologyTargetKind)) {
        if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
            SectorAuthoringSideId sideId{
                    state.texturePicker.authoringLineId,
                    state.texturePicker.authoringSide};
            if (!IsValidSectorAuthoringId(sideId.lineId)) {
                FindSectorEditorAuthoringSideIdForTopologySideDef(
                        state,
                        state.texturePicker.topologySideDefId,
                        sideId);
            }
            if (IsValidSectorAuthoringId(sideId.lineId)) {
                const SectorAuthoringLineSide* authoringSide =
                        FindSectorAuthoringLineSide(state.authoringGraph, sideId);
                if (authoringSide != nullptr) {
                    const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                            *authoringSide,
                            state.texturePicker.topologyWallPart);
                    const TopologyMaterialLayer layer = state.texturePicker.topologyWallPart == TopologyWallPart::Middle
                            ? TopologyMaterialLayer::Base
                            : state.texturePicker.topologyLayer;
                    return layer == TopologyMaterialLayer::Decal
                            ? part.decal.textureId
                            : part.textureId;
                }
            }
        } else {
            const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                    state.topologyMap,
                    state.texturePicker.topologySideDefId);
            if (sideDef != nullptr) {
                const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                        *sideDef,
                        state.texturePicker.topologyWallPart);
                const TopologyMaterialLayer layer = state.texturePicker.topologyWallPart == TopologyWallPart::Middle
                        ? TopologyMaterialLayer::Base
                        : state.texturePicker.topologyLayer;
                return layer == TopologyMaterialLayer::Decal
                        ? part.decal.textureId
                        : part.textureId;
            }
        }
        return std::string{};
    }

    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor) {
        int faceAnchorId = state.texturePicker.authoringFaceAnchorId;
        if (!IsValidSectorAuthoringId(faceAnchorId)) {
            faceAnchorId = FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    state,
                    state.texturePicker.topologySectorId);
        }
        const SectorAuthoringFaceAnchor* anchor =
                FindSectorAuthoringFaceAnchor(state.authoringGraph, faceAnchorId);
        if (anchor == nullptr) {
            return std::string{};
        }
        switch (state.texturePicker.topologyField) {
            case TopologySectorTextureField::Floor:
                return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->floorDecal.textureId
                        : anchor->floorTextureId;
            case TopologySectorTextureField::Ceiling:
                return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->ceilingDecal.textureId
                        : anchor->ceilingTextureId;
            case TopologySectorTextureField::DefaultWall:
                return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultWall.decal.textureId
                        : anchor->defaultWall.textureId;
            case TopologySectorTextureField::DefaultLower:
                return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultLower.decal.textureId
                        : anchor->defaultLower.textureId;
            case TopologySectorTextureField::DefaultUpper:
                return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                        ? anchor->defaultUpper.decal.textureId
                        : anchor->defaultUpper.textureId;
            case TopologySectorTextureField::None: break;
        }
        return std::string{};
    }

    const SectorTopologySector* sector = FindSectorTopologySector(
            state.topologyMap,
            state.texturePicker.topologySectorId);
    if (sector == nullptr) {
        return std::string{};
    }
    switch (state.texturePicker.topologyField) {
        case TopologySectorTextureField::Floor:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->floorDecal.textureId
                    : sector->floorTextureId;
        case TopologySectorTextureField::Ceiling:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->ceilingDecal.textureId
                    : sector->ceilingTextureId;
        case TopologySectorTextureField::DefaultWall: return sector->defaultWall.textureId;
        case TopologySectorTextureField::DefaultLower: return sector->defaultLower.textureId;
        case TopologySectorTextureField::DefaultUpper: return sector->defaultUpper.textureId;
        case TopologySectorTextureField::None: break;
    }
    return std::string{};
}

bool OpenTopologyTexturePicker(
        SectorEditorState& state,
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (FindSectorTopologySector(state.topologyMap, sectorId) == nullptr
            || field == TopologySectorTextureField::None
            || (layer == TopologyMaterialLayer::Decal
                    && !IsAuthoringFaceAnchorDecalTextureField(field))) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::Sector;
    picker.topologyLayer = layer;
    picker.topologySectorId = sectorId;
    picker.topologyField = field;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenTopologySideDefTexturePicker(
        SectorEditorState& state,
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    if (sideDef == nullptr
            || (wallPart == TopologyWallPart::Middle
                    && !IsTopologyMiddleEligible(state.topologyMap, sideDef))) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::SideDef;
    picker.topologyLayer = wallPart == TopologyWallPart::Middle
            ? TopologyMaterialLayer::Base
            : layer;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = sideDefId;
    picker.topologyWallPart = wallPart;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenAuthoringFaceAnchorTexturePicker(
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
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
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

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenAuthoringFaceAnchorTexturePickerById(
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
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
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
        picker = TexturePickerState{};
        return false;
    }

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenAuthoringSideTexturePicker(
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
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
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

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenAuthoringSideTexturePickerById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (!HasCurrentAuthoringDerivation(state)
            || FindSectorAuthoringLine(state.authoringGraph, sideId.lineId) == nullptr) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
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
        picker = TexturePickerState{};
        return false;
    }

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenMapSkyTexturePicker(SectorEditorState& state)
{
    TexturePickerState& picker = state.texturePicker;
    if (!state.previewSettingsModal.open) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::MapSky;
    picker.topologyLayer = TopologyMaterialLayer::Base;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(SectorEditorState& state)
{
    SectorEditorTexturePickerApplyResult result;
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::None
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        picker = TexturePickerState{};
        return result;
    }

    const std::string selectedTexture = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        if (state.previewSettingsModal.open
                && state.previewSettingsModal.draftSkySettings.textureId != selectedTexture) {
            state.previewSettingsModal.draftSkySettings.textureId = selectedTexture;
            state.previewSettingsModal.errorMessage.clear();
        }
        picker = TexturePickerState{};
        return result;
    }

    AssignSelectedTextureToPickerTarget(state.topologyMap, picker, selectedTexture, result);

    picker = TexturePickerState{};
    return result;
}

SectorEditorTexturePickerApplyResult ApplyAuthoringTexturePickerSelection(SectorEditorState& state)
{
    TexturePickerState picker = state.texturePicker;
    if (!picker.open
            || (picker.topologyTargetKind != TopologyTexturePickerTargetKind::AuthoringFaceAnchor
                    && picker.topologyTargetKind != TopologyTexturePickerTargetKind::AuthoringSide)) {
        return ApplyTexturePickerSelection(state);
    }

    SectorEditorTexturePickerApplyResult result;
    const auto closeAndReturn = [&state, &result]() {
        state.texturePicker = TexturePickerState{};
        return result;
    };

    if (picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        return closeAndReturn();
    }

    const std::string selectedTexture =
            picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
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

        SectorTopologyMap editedTopology = state.topologyMap;
        AssignSelectedTextureToPickerTarget(editedTopology, picker, selectedTexture, result);
        if (!result.changed) {
            return closeAndReturn();
        }

        const SectorTopologySector* editedSector =
                FindSectorTopologySector(editedTopology, picker.topologySectorId);
        if (editedSector == nullptr) {
            result.changed = false;
            result.status = "Authoring face texture edit unavailable: selected sector is not current";
            return closeAndReturn();
        }

        const SectorTopologySector copiedSector = *editedSector;
        if (!MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                    state,
                    picker.topologySectorId,
                    result.status.c_str(),
                    [picker, copiedSector](SectorAuthoringFaceAnchor& anchor) {
                        switch (picker.topologyField) {
                            case TopologySectorTextureField::Floor:
                                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                    anchor.floorDecal = copiedSector.floorDecal;
                                } else {
                                    anchor.floorTextureId = copiedSector.floorTextureId;
                                }
                                return true;
                            case TopologySectorTextureField::Ceiling:
                                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                                    anchor.ceilingDecal = copiedSector.ceilingDecal;
                                } else {
                                    anchor.ceilingTextureId = copiedSector.ceilingTextureId;
                                }
                                return true;
                            case TopologySectorTextureField::DefaultWall:
                                anchor.defaultWall = copiedSector.defaultWall;
                                return true;
                            case TopologySectorTextureField::DefaultLower:
                                anchor.defaultLower = copiedSector.defaultLower;
                                return true;
                            case TopologySectorTextureField::DefaultUpper:
                                anchor.defaultUpper = copiedSector.defaultUpper;
                                return true;
                            case TopologySectorTextureField::None:
                                break;
                        }
                        return false;
                    })) {
            result.changed = false;
        }
        return closeAndReturn();
    }

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

    SectorTopologyMap editedTopology = state.topologyMap;
    AssignSelectedTextureToPickerTarget(editedTopology, picker, selectedTexture, result);
    if (!result.changed) {
        return closeAndReturn();
    }

    const SectorTopologySideDef* editedSideDef =
            FindSectorTopologySideDef(editedTopology, picker.topologySideDefId);
    if (editedSideDef == nullptr) {
        result.changed = false;
        result.status = "Authoring side texture edit unavailable: selected sidedef is not current";
        return closeAndReturn();
    }

    const SectorTopologyWallPartSettings copiedPart =
            TopologyWallPartSettingsFor(*editedSideDef, picker.topologyWallPart);
    if (!MutateSectorEditorAuthoringSideForTopologySideDef(
                state,
                picker.topologySideDefId,
                result.status.c_str(),
                [picker, copiedPart](SectorAuthoringLineSide& side) {
                    TopologyWallPartSettingsFor(side, picker.topologyWallPart) = copiedPart;
                    return true;
                })) {
        result.changed = false;
    }
    return closeAndReturn();
}

} // namespace game
