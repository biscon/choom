#include "sector_editor/SectorEditorTextureModals.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace game {

namespace {

bool ContainsClipName(const std::vector<std::string>& clipNames, const std::string& clipName)
{
    return !clipName.empty()
            && std::find(clipNames.begin(), clipNames.end(), clipName) != clipNames.end();
}

std::string FirstAvailableClipName(const SectorSpriteMetadata& metadata)
{
    return metadata.clipNames.empty() ? std::string{} : metadata.clipNames.front();
}

std::string ChooseBillboardClipName(
        const SectorSpriteMetadata& metadata,
        const std::string& preferred,
        const std::string& current,
        bool repairInvalidNonEmpty)
{
    if (ContainsClipName(metadata.clipNames, current)) {
        return current;
    }
    if (!current.empty() && !repairInvalidNonEmpty) {
        return current;
    }
    if (ContainsClipName(metadata.clipNames, preferred)) {
        return preferred;
    }
    return FirstAvailableClipName(metadata);
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

void RefreshSpritePickerScan(SpritePickerState& picker)
{
    picker.sprites = ScanAssetSpriteAsepriteJsons(picker.scanMessage);
    picker.spriteOptionLabels.clear();
    picker.spriteOptionLabels.reserve(picker.sprites.size());
    for (const SectorSpriteMetadata& metadata : picker.sprites) {
        picker.spriteOptionLabels.push_back(metadata.spriteAnimationPath.c_str());
    }
    picker.scanned = true;
    picker.spriteScroll = engine::UIScrollState{};
    int selectedIndex = picker.sprites.empty() ? -1 : 0;
    if (!picker.requestedSpriteAnimationPath.empty()) {
        for (size_t i = 0; i < picker.sprites.size(); ++i) {
            if (picker.sprites[i].spriteAnimationPath == picker.requestedSpriteAnimationPath) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
    }
    SelectSpritePickerSprite(picker, selectedIndex);
}

void RefreshSpriteMetadataCatalog(SectorSpriteMetadataCatalog& catalog)
{
    catalog.sprites = ScanAssetSpriteAsepriteJsons(catalog.scanMessage);
    catalog.scanned = true;
}

const SectorSpriteMetadata* FindSpriteMetadata(
        const SectorSpriteMetadataCatalog& catalog,
        const std::string& spriteAnimationPath)
{
    if (spriteAnimationPath.empty()) {
        return nullptr;
    }
    const auto it = std::find_if(
            catalog.sprites.begin(),
            catalog.sprites.end(),
            [&spriteAnimationPath](const SectorSpriteMetadata& metadata) {
                return metadata.spriteAnimationPath == spriteAnimationPath;
            });
    return it == catalog.sprites.end() ? nullptr : &(*it);
}

void SelectSpritePickerSprite(SpritePickerState& picker, int spriteIndex)
{
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(picker.sprites.size())) {
        picker.selectedSpriteIndex = -1;
        picker.previewTexture = engine::NullTextureHandle();
        picker.previewAtlasPath.clear();
        return;
    }

    picker.selectedSpriteIndex = spriteIndex;
    picker.previewTexture = engine::NullTextureHandle();
    picker.previewAtlasPath.clear();
}

SectorEditorSpritePickerResult SelectedSpritePickerResult(const SpritePickerState& picker)
{
    SectorEditorSpritePickerResult result;
    if (picker.selectedSpriteIndex < 0 || picker.selectedSpriteIndex >= static_cast<int>(picker.sprites.size())) {
        return result;
    }

    const SectorSpriteMetadata& metadata = picker.sprites[static_cast<size_t>(picker.selectedSpriteIndex)];
    result.valid = true;
    result.spriteAnimationPath = metadata.spriteAnimationPath;
    result.atlasImagePath = metadata.atlasImagePath;
    result.clipNames = metadata.clipNames;
    return result;
}

void OpenBillboardSpritePicker(
        SpritePickerState& picker,
        const std::string& spriteAnimationPath)
{
    picker.open = true;
    picker.scanned = false;
    picker.scanMessage.clear();
    picker.spriteScroll = engine::UIScrollState{};
    picker.selectedSpriteIndex = -1;
    picker.requestedSpriteAnimationPath = spriteAnimationPath;
    picker.previewTexture = engine::NullTextureHandle();
    picker.previewAtlasPath.clear();
}

bool ApplySpritePickerResultToBillboard(
        SectorPlacedBillboard& billboard,
        const SectorEditorSpritePickerResult& result)
{
    if (!result.valid || result.spriteAnimationPath.empty()) {
        return false;
    }

    SectorSpriteMetadata metadata;
    metadata.spriteAnimationPath = result.spriteAnimationPath;
    metadata.atlasImagePath = result.atlasImagePath;
    metadata.clipNames = result.clipNames;

    SectorPlacedBillboard edited = billboard;
    edited.spriteAnimationPath = result.spriteAnimationPath;
    RepairBillboardClipsForSpriteMetadata(edited, metadata, true);

    const bool changed = edited.spriteAnimationPath != billboard.spriteAnimationPath
            || edited.clip != billboard.clip
            || edited.frontClip != billboard.frontClip
            || edited.backClip != billboard.backClip
            || edited.leftClip != billboard.leftClip
            || edited.rightClip != billboard.rightClip;
    if (changed) {
        billboard = std::move(edited);
    }
    return changed;
}

bool RepairBillboardClipsForSpriteMetadata(
        SectorPlacedBillboard& billboard,
        const SectorSpriteMetadata& metadata,
        bool repairInvalidNonEmpty)
{
    if (metadata.clipNames.empty()) {
        return false;
    }

    SectorPlacedBillboard edited = billboard;
    edited.clip = ChooseBillboardClipName(metadata, "Default", edited.clip, repairInvalidNonEmpty);
    edited.frontClip = ChooseBillboardClipName(metadata, "Front", edited.frontClip, repairInvalidNonEmpty);
    edited.backClip = ChooseBillboardClipName(metadata, "Back", edited.backClip, repairInvalidNonEmpty);
    edited.leftClip = ChooseBillboardClipName(metadata, "Left", edited.leftClip, repairInvalidNonEmpty);
    edited.rightClip = ChooseBillboardClipName(metadata, "Right", edited.rightClip, repairInvalidNonEmpty);

    const bool changed = edited.clip != billboard.clip
            || edited.frontClip != billboard.frontClip
            || edited.backClip != billboard.backClip
            || edited.leftClip != billboard.leftClip
            || edited.rightClip != billboard.rightClip;
    if (changed) {
        billboard = std::move(edited);
    }
    return changed;
}

std::string CurrentTextureForPickerTarget(const SectorEditorState& state)
{
    if (IsSectorEditorMaterialTexturePickerTarget(state.texturePicker.topologyTargetKind)) {
        return CurrentSectorEditorMaterialPickerTexture(state, state.texturePicker);
    }
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        return state.previewSettingsModal.open
                ? state.previewSettingsModal.draftSkySettings.textureId
                : state.topologyMap.skySettings.textureId;
    }
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDoor) {
        const SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(state.topologyMap, state.texturePicker.runtimeObjectId);
        return object != nullptr && object->kind == "door" ? object->door.textureId : std::string{};
    }

    return std::string{};
}

bool OpenTopologyTexturePicker(
        SectorEditorState& state,
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForSector(state, sectorId, field, layer);
}

bool OpenTopologySideDefTexturePicker(
        SectorEditorState& state,
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForSideDef(state, sideDefId, wallPart, layer);
}

bool OpenAuthoringFaceAnchorTexturePicker(
        SectorEditorState& state,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(state, topologySectorId, field, layer);
}

bool OpenAuthoringFaceAnchorTexturePickerById(
        SectorEditorState& state,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringFaceAnchorById(state, faceAnchorId, field, layer);
}

bool OpenAuthoringSideTexturePicker(
        SectorEditorState& state,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSide(state, topologySideDefId, wallPart, layer);
}

bool OpenAuthoringSideTexturePickerById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    return OpenSectorEditorMaterialPickerForAuthoringSideById(state, sideId, wallPart, layer);
}

bool OpenMapSkyTexturePicker(SectorEditorState& state)
{
    TexturePickerState& picker = state.texturePicker;
    if (!state.previewSettingsModal.open) {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::MapSky;
    picker.topologyLayer = TopologyMaterialLayer::Base;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;

    OpenSectorEditorTexturePicker(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenRuntimeDoorTexturePicker(SectorEditorState& state, int runtimeObjectId)
{
    TexturePickerState& picker = state.texturePicker;
    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(state.topologyMap, runtimeObjectId);
    if (object == nullptr || object->kind != "door") {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::RuntimeDoor;
    picker.topologyLayer = TopologyMaterialLayer::Base;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;
    picker.authoringFaceAnchorId = -1;
    picker.authoringLineId = -1;
    picker.authoringSide = SectorTopologySideKind::Front;
    picker.runtimeObjectId = runtimeObjectId;

    OpenSectorEditorTexturePicker(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(SectorEditorState& state)
{
    SectorEditorTexturePickerApplyResult result;
    TexturePickerState& picker = state.texturePicker;
    if (IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return ApplySectorEditorMaterialTexturePickerSelection(state);
    }

    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    const std::string selectedTexture = selected.textureId;
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        if (state.previewSettingsModal.open
                && state.previewSettingsModal.draftSkySettings.textureId != selectedTexture) {
            state.previewSettingsModal.draftSkySettings.textureId = selectedTexture;
            state.previewSettingsModal.errorMessage.clear();
        }
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDoor) {
        SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(state.topologyMap, picker.runtimeObjectId);
        if (object != nullptr && object->kind == "door" && object->door.textureId != selectedTexture) {
            object->door.textureId = selectedTexture;
            result.changed = true;
            result.status = "Selected door texture.";
        }
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    CloseSectorEditorTexturePicker(picker);
    return result;
}

SectorEditorTexturePickerApplyResult ApplyAuthoringTexturePickerSelection(SectorEditorState& state)
{
    return ApplySectorEditorMaterialTexturePickerSelection(state);
}

} // namespace game
