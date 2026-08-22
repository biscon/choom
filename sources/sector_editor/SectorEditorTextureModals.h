#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace game {

class SectorEditorTextureCatalogService;

struct SectorEditorTexturePickerCallbacks {
    std::function<void()> close;
    std::function<void()> applySelection;
    std::function<std::string()> currentTextureForTarget;
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
    std::vector<std::string> clipNames;
};

void DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        TexturePickerState& picker,
        SectorEditorTextureCatalogService& textureCatalog,
        const SectorEditorTexturePickerCallbacks& callbacks);

void DrawSpritePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SpritePickerState& picker,
        const SectorEditorSpritePickerCallbacks& callbacks);

void RefreshSpritePickerScan(SpritePickerState& picker);
void RefreshSpriteMetadataCatalog(SectorSpriteMetadataCatalog& catalog);
const SectorSpriteMetadata* FindSpriteMetadata(
        const SectorSpriteMetadataCatalog& catalog,
        const std::string& spriteAnimationPath);
void SelectSpritePickerSprite(SpritePickerState& picker, int spriteIndex);
void RefreshSpritePickerPreview(SpritePickerState& picker, engine::AssetManager& assets);
void CloseSpritePicker(SpritePickerState& picker, engine::AssetManager& assets);
SectorEditorSpritePickerResult SelectedSpritePickerResult(const SpritePickerState& picker);
void OpenBillboardSpritePicker(
        SpritePickerState& picker,
        const std::string& spriteAnimationPath);
bool ApplySpritePickerResultToBillboard(
        SectorPlacedBillboard& billboard,
        const SectorEditorSpritePickerResult& result);
bool RepairBillboardClipsForSpriteMetadata(
        SectorPlacedBillboard& billboard,
        const SectorSpriteMetadata& metadata,
        bool repairInvalidNonEmpty);

bool OpenMapSkyTexturePicker(
        SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorTextureCatalogService& textureCatalog);
bool OpenRuntimeDoorTexturePicker(
        SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorTextureCatalogService& textureCatalog,
        int runtimeObjectId);
std::string CurrentTextureForPickerTarget(
        const SectorEditorState& state,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation);
SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation);

} // namespace game
