#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

namespace game {

using SectorEditorMaterialPickerActionFn =
        std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>;

struct SectorEditorMaterialPickerRoutingContext {
    SectorEditorState& state;
    std::string& statusText;
    std::function<bool(const char*, engine::AssetManager*)> finishTopologyMaterialMutation;
    std::function<bool(
            TopologySurfaceEditTarget,
            const SectorEditorMaterialActionResult&,
            const SectorTopologyMap&,
            engine::AssetManager*)> finishAuthoringSideMaterialActionResult;
    std::function<bool(
            TopologySurfaceEditTarget,
            engine::AssetManager*,
            SectorEditorMaterialPickerActionFn)> applyAuthoringFaceAnchorFlatMaterialAction;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool()> rebuildPreviewForTexturePickerApply;
};

bool IsSectorEditorMaterialTexturePickerTarget(TopologyTexturePickerTargetKind kind);

std::string CurrentSectorEditorMaterialPickerTexture(
        const SectorEditorState& state,
        const TexturePickerState& picker);

bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        SectorEditorState& state,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchorById(
        SectorEditorState& state,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringSide(
        SectorEditorState& state,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringSideById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(SectorEditorState& state);
SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        SectorEditorMaterialPickerRoutingContext& context,
        engine::AssetManager* assets);

} // namespace game
