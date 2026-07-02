#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace game {

struct SectorEditorAddTextureModalCallbacks {
    std::function<void()> close;
    std::function<bool()> addSelected;
    std::function<void(int)> selectPath;
    std::function<void()> refreshPreview;
    std::function<bool(std::string&)> validateId;
};

struct SectorEditorTexturePickerCallbacks {
    std::function<void()> close;
    std::function<void()> applySelection;
    std::function<std::string()> currentTextureForTarget;
    std::function<engine::TextureHandle(const std::string&)> textureHandleForId;
};

struct SectorEditorSpritePickerCallbacks {
    std::function<void()> close;
    std::function<void()> applySelection;
    std::function<void()> refreshScan;
    std::function<void(int)> selectSprite;
    std::function<void()> refreshPreview;
};

struct SectorEditorTexturePickerApplyResult {
    bool changed = false;
    bool useMaterialMutationFinish = false;
    bool rebuildPreviewOnApply = false;
    std::string status;
};

struct SectorEditorSpritePickerResult {
    bool valid = false;
    std::string spriteAnimationPath;
    std::string atlasImagePath;
    std::string clipName;
    std::vector<std::string> clipNames;
};

struct SectorEditorAddTextureResult {
    bool success = false;
    bool replacing = false;
    std::string textureId;
    std::string error;
};

void DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        AddMapTextureState& modalState,
        const SectorEditorAddTextureModalCallbacks& callbacks);

void DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const SectorEditorTexturePickerCallbacks& callbacks);

void DrawSpritePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SpritePickerState& picker,
        const SectorEditorSpritePickerCallbacks& callbacks);

void RefreshAddMapTextureScan(AddMapTextureState& modalState);
void SelectAddMapTexturePath(
        AddMapTextureState& modalState,
        const SectorTopologyMap& map,
        int pathIndex);
void RefreshAddMapTexturePreview(AddMapTextureState& modalState, engine::AssetManager& assets);
bool ValidateAddMapTextureId(const AddMapTextureState& modalState, std::string& error);
SectorEditorAddTextureResult AddSelectedMapTexture(SectorEditorState& state);

void RefreshSpritePickerScan(SpritePickerState& picker);
void SelectSpritePickerSprite(SpritePickerState& picker, int spriteIndex);
void RefreshSpritePickerPreview(SpritePickerState& picker, engine::AssetManager& assets);
void CloseSpritePicker(SpritePickerState& picker, engine::AssetManager& assets);
SectorEditorSpritePickerResult SelectedSpritePickerResult(const SpritePickerState& picker);
void OpenBillboardSpritePicker(
        SpritePickerState& picker,
        BillboardSpritePickerTarget target,
        const std::string& spriteAnimationPath,
        const std::string& clipName);
bool ApplySpritePickerResultToBillboard(
        SectorPlacedBillboard& billboard,
        BillboardSpritePickerTarget target,
        const SectorEditorSpritePickerResult& result);

bool OpenTopologyTexturePicker(
        SectorEditorState& state,
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenTopologySideDefTexturePicker(
        SectorEditorState& state,
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenAuthoringFaceAnchorTexturePicker(
        SectorEditorState& state,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenAuthoringFaceAnchorTexturePickerById(
        SectorEditorState& state,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenAuthoringSideTexturePicker(
        SectorEditorState& state,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenAuthoringSideTexturePickerById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenMapSkyTexturePicker(SectorEditorState& state);
std::string CurrentTextureForPickerTarget(const SectorEditorState& state);
SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(SectorEditorState& state);
SectorEditorTexturePickerApplyResult ApplyAuthoringTexturePickerSelection(SectorEditorState& state);

} // namespace game
