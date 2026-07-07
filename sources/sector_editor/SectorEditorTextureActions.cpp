#include "sector_editor/SectorEditorTextureModals.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>
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

bool OpenMapSkyTexturePicker(
        SectorEditorState& state,
        SectorEditorTextureCatalogService& textureCatalog)
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

    OpenSectorEditorTexturePicker(
            picker,
            textureCatalog.TextureIds(),
            CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenRuntimeDoorTexturePicker(
        SectorEditorState& state,
        SectorEditorTextureCatalogService& textureCatalog,
        int runtimeObjectId)
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

    OpenSectorEditorTexturePicker(
            picker,
            textureCatalog.TextureIds(),
            CurrentTextureForPickerTarget(state));
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

} // namespace game
