#include "sector_editor/SectorEditorAuthoringState.h"

#include <raylib.h>

#include <utility>

namespace game {
namespace {

void CopyEditorMapLevelFields(SectorTopologyMap& target, const SectorTopologyMap& source)
{
    target.texturesById = source.texturesById;
    target.staticLights = source.staticLights;
    target.previewSettings = source.previewSettings;
    target.skySettings = source.skySettings;
    target.directionalLight = source.directionalLight;
    target.lightmapSettings = source.lightmapSettings;
    target.bakedLightmap = {};
}

void InvalidateEditorTopologyRenderCache(SectorEditorState& state)
{
    state.topologyRenderCache.valid = false;
    ++state.topologyRenderRevision;
}

} // namespace

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorEditorState& state,
        const SectorTopologyMap& sourceMap)
{
    state.authoringGraph = ImportSectorTopologyMapToAuthoringGraph(sourceMap);
    state.authoringDerivation = DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
    if (state.authoringDerivation.success) {
        CopyEditorMapLevelFields(state.authoringDerivation.topology, sourceMap);
        state.lastValidAuthoringDerivedTopology = sourceMap;
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        state.authoringDerivedTopologyStale = false;
        state.authoringDerivationStatus = "Authoring graph: derived topology current";
    } else {
        state.lastValidAuthoringDerivedTopology.reset();
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        state.authoringDerivedTopologyStale = true;
        state.authoringDerivationStatus = "Authoring graph: no valid derived topology";
    }
}

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        const char* status)
{
    state.topologyDocumentDirty = true;
    state.authoringDerivedTopologyStale = true;
    state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::ValidStale
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    state.authoringDerivationStatus = status == nullptr || status[0] == '\0'
            ? "Authoring graph edited; derived topology stale"
            : status;
    state.topologyDocumentStatus = state.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCache(state);
}

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        const char* successStatus,
        const char* failureStatus)
{
    SectorAuthoringDerivationResult result =
            DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
    if (result.success) {
        CopyEditorMapLevelFields(result.topology, state.topologyMap);
        state.topologyMap = result.topology;
        state.lastValidAuthoringDerivedTopology = result.topology;
        state.authoringDerivation = std::move(result);
        state.authoringDerivedTopologyStale = false;
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        state.authoringDerivationStatus = successStatus == nullptr || successStatus[0] == '\0'
                ? "Authoring graph: derived topology current"
                : successStatus;
        state.topologyDocumentStatus = state.authoringDerivationStatus;
        InvalidateEditorTopologyRenderCache(state);
        return true;
    }

    state.authoringDerivation = std::move(result);
    state.authoringDerivedTopologyStale = true;
    state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::InvalidLastValid
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    const int diagnosticCount = static_cast<int>(state.authoringDerivation.diagnostics.size());
    state.authoringDerivationStatus = failureStatus == nullptr || failureStatus[0] == '\0'
            ? TextFormat("Authoring graph: derivation failed (%d diagnostics)", diagnosticCount)
            : failureStatus;
    state.topologyDocumentStatus = state.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCache(state);
    return false;
}

} // namespace game
