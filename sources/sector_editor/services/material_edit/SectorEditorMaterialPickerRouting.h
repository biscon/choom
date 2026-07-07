#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/document/SectorEditorDocumentState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorMaterialPickerRoutingContext {
    TexturePickerState& texturePicker;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    std::string& statusText;
    std::function<bool()> rebuildPreviewForTexturePickerApply;
};

bool IsSectorEditorMaterialTexturePickerTarget(TopologyTexturePickerTargetKind kind);

std::string CurrentSectorEditorMaterialPickerTexture(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        const TexturePickerState& picker);
std::string CurrentSectorEditorMaterialPickerTexture(
        const SectorTopologyMap& topologyMap,
        SectorEditorConstAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        const TexturePickerState& picker);

bool OpenSectorEditorMaterialPickerForDerivedSector(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForDerivedSector(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int faceAnchorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForDerivedSideDef(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForDerivedSideDef(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        int topologySideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringSide(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);
bool OpenSectorEditorMaterialPickerForAuthoringSide(
        TexturePickerState& picker,
        const SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorConstDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer);

SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        TexturePickerState& texturePicker,
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation);
SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        TexturePickerState& texturePicker,
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorEditorAuthoringDocumentAccess authoring,
        SectorEditorDerivationDocumentAccess derivation);
SectorEditorTexturePickerApplyResult ApplySectorEditorMaterialTexturePickerSelection(
        SectorEditorMaterialPickerRoutingContext& context,
        engine::AssetManager* assets);

} // namespace game
