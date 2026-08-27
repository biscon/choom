#pragma once

#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorState.h"
#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"

#include <string>

namespace game {

class SectorEditorPatrolEditorService {
public:
    SectorEditorPatrolEditorService(
            SectorEditorPatrolEditorState& state,
            SectorAuthoringGraph& graph,
            SectorTopologyMap& map,
            SectorEditorDerivationDocumentAccess derivation,
            SectorEditorDocumentLifecycleAccess lifecycle,
            uint64_t& topologyRenderRevision,
            SectorEditorTopologyRenderCache& topologyRenderCache,
            std::string& statusText);

    void Open();
    void Cancel();
    bool SaveAndClose();

    SectorAuthoringPatrol* Selected();
    const SectorAuthoringPatrol* Selected() const;
    bool SelectIndex(int index);
    void AddPatrol();
    bool RequestDeleteSelected();
    void CancelDelete();
    bool ConfirmDeleteSelected();
    bool ApplyIdBuffer();
    bool SetMode(SectorPatrolMode mode);
    bool SetShuffleWaypoints(bool enabled);
    bool AddWaypoint();
    bool RemoveWaypoint(size_t index);
    bool MoveWaypoint(size_t index, int direction);
    bool SetWaypointMarker(size_t index, int markerId);
    bool SetWaypointDelaySeconds(size_t index, float seconds);
    bool SetWaypointGait(size_t index, SectorPatrolGait gait);
    bool SetWaypointLookAround(size_t index, bool enabled);
    bool SetWaypointLookArc(size_t index, float degrees);

    bool SelectedIsAssigned() const;
    std::string SelectedUsageText() const;

    SectorEditorPatrolEditorState& State() { return state_; }
    const SectorEditorPatrolEditorState& State() const { return state_; }

private:
    void Close();
    void SyncSelection();
    void RebuildMarkerOptions();
    void RebuildListLabels();
    bool ValidateDrafts(std::string& error) const;

    SectorEditorPatrolEditorState& state_;
    SectorAuthoringGraph& graph_;
    SectorTopologyMap& map_;
    SectorEditorDerivationDocumentAccess derivation_;
    SectorEditorDocumentLifecycleAccess lifecycle_;
    uint64_t& topologyRenderRevision_;
    SectorEditorTopologyRenderCache& topologyRenderCache_;
    std::string& statusText_;
};

} // namespace game
