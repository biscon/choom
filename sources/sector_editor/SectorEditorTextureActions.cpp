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

std::string CurrentTextureForPickerTarget(
        const SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation)
{
    if (IsSectorEditorMaterialTexturePickerTarget(state.texturePicker.topologyTargetKind)) {
        return CurrentSectorEditorMaterialPickerTexture(
                topologyMap,
                authoringGraph,
                derivation,
                state.texturePicker);
    }
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        return state.previewSettingsModal.open
                ? state.previewSettingsModal.draftSkySettings.materialId
                : topologyMap.skySettings.materialId;
    }
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDoor) {
        const SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(topologyMap, state.texturePicker.runtimeObjectId);
        return object != nullptr && object->kind == "door" ? object->door.materialId : std::string{};
    }
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDuctFrame
            || state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDuctLouvers) {
        const SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(topologyMap, state.texturePicker.runtimeObjectId);
        if (object == nullptr || object->kind != "duct_access") return {};
        return state.texturePicker.topologyTargetKind
                        == TopologyTexturePickerTargetKind::RuntimeDuctLouvers
                ? object->ductAccess.cover.louverMaterialId
                : object->ductAccess.cover.frameMaterialId;
    }

    return std::string{};
}

bool OpenMapSkyTexturePicker(
        SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
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
            state.previewSettingsModal.open
                    ? state.previewSettingsModal.draftSkySettings.materialId
                    : topologyMap.skySettings.materialId);
    return true;
}

bool OpenRuntimeDoorTexturePicker(
        SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorTextureCatalogService& textureCatalog,
        int runtimeObjectId)
{
    TexturePickerState& picker = state.texturePicker;
    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(topologyMap, runtimeObjectId);
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
            object->door.materialId);
    return true;
}

bool OpenRuntimeDuctTexturePicker(
        SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorTextureCatalogService& textureCatalog,
        int runtimeObjectId,
        bool louvers)
{
    (void)authoringGraph;
    TexturePickerState& picker = state.texturePicker;
    const SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(topologyMap, runtimeObjectId);
    if (object == nullptr || object->kind != "duct_access") {
        CloseSectorEditorTexturePicker(picker);
        return false;
    }

    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = louvers
            ? TopologyTexturePickerTargetKind::RuntimeDuctLouvers
            : TopologyTexturePickerTargetKind::RuntimeDuctFrame;
    picker.topologyLayer = TopologyMaterialLayer::Base;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;
    picker.authoringFaceAnchorId = -1;
    picker.authoringLineId = -1;
    picker.runtimeObjectId = runtimeObjectId;
    OpenSectorEditorTexturePicker(
            picker,
            textureCatalog.TextureIds(),
            louvers
                    ? object->ductAccess.cover.louverMaterialId
                    : object->ductAccess.cover.frameMaterialId);
    return true;
}

SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation)
{
    SectorEditorTexturePickerApplyResult result;
    TexturePickerState& picker = state.texturePicker;
    if (IsSectorEditorMaterialTexturePickerTarget(picker.topologyTargetKind)) {
        return ApplySectorEditorMaterialTexturePickerSelection(
                state.texturePicker,
                lifecycle,
                state.topologyRenderRevision,
                state.topologyRenderCache,
                topologyMap,
                authoringGraph,
                derivation);
    }

    const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
    if (!selected.valid) {
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    const std::string selectedTexture = selected.materialId;
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        if (state.previewSettingsModal.open
                && state.previewSettingsModal.draftSkySettings.materialId != selectedTexture) {
            state.previewSettingsModal.draftSkySettings.materialId = selectedTexture;
            state.previewSettingsModal.errorMessage.clear();
        }
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDoor) {
        SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(topologyMap, picker.runtimeObjectId);
        if (object != nullptr && object->kind == "door" && object->door.materialId != selectedTexture) {
            object->door.materialId = selectedTexture;
            result.changed = true;
            result.status = "Selected door texture.";
        }
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDuctFrame
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDuctLouvers) {
        SectorPlacedRuntimeObject* object =
                FindSectorPlacedRuntimeObject(topologyMap, picker.runtimeObjectId);
        if (object != nullptr && object->kind == "duct_access") {
            std::string& material = picker.topologyTargetKind
                            == TopologyTexturePickerTargetKind::RuntimeDuctLouvers
                    ? object->ductAccess.cover.louverMaterialId
                    : object->ductAccess.cover.frameMaterialId;
            if (material != selectedTexture) {
                material = selectedTexture;
                result.changed = true;
                result.status = "Selected vent cover material.";
            }
        }
        CloseSectorEditorTexturePicker(picker);
        return result;
    }

    CloseSectorEditorTexturePicker(picker);
    return result;
}

} // namespace game
