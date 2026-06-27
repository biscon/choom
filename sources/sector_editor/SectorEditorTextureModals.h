#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>

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

struct SectorEditorTexturePickerApplyResult {
    bool changed = false;
    bool useMaterialMutationFinish = false;
    bool rebuildPreviewOnApply = false;
    std::string status;
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

void RefreshAddMapTextureScan(AddMapTextureState& modalState);
void SelectAddMapTexturePath(
        AddMapTextureState& modalState,
        const SectorTopologyMap& map,
        int pathIndex);
void RefreshAddMapTexturePreview(AddMapTextureState& modalState, engine::AssetManager& assets);
bool ValidateAddMapTextureId(const AddMapTextureState& modalState, std::string& error);
SectorEditorAddTextureResult AddSelectedMapTexture(SectorEditorState& state);

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
bool OpenMapSkyTexturePicker(SectorEditorState& state);
std::string CurrentTextureForPickerTarget(const SectorEditorState& state);
SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(SectorEditorState& state);

} // namespace game
