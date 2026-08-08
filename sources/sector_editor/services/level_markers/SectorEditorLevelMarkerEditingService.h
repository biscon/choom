#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorLevelMarkerEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    LevelMarkerEditingState& editingState;
    std::string& statusText;
};

class SectorEditorLevelMarkerEditingService {
public:
    explicit SectorEditorLevelMarkerEditingService(
            SectorEditorLevelMarkerEditingServiceContext context);

    SectorAuthoringLevelMarker* Selected();
    const SectorAuthoringLevelMarker* Selected() const;
    bool Place(Vector2 snappedMapPoint, int* outId = nullptr);
    bool ValidateSelectedReferenceId(
            const std::string& referenceId,
            std::string& error) const;
    bool RenameSelected(const std::string& referenceId);
    bool SetSelectedPosition(Vector3 authoringPosition);
    bool SetSelectedOrientation(float degrees);
    bool DeleteSelected();

    bool BeginMove(int markerId);
    void UpdateMove(Vector2 snappedMapPoint);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    const LevelMarkerDragState& Drag() const { return context_.editingState.drag; }

private:
    bool MutateSelected(
            const char* status,
            const std::function<bool(SectorAuthoringLevelMarker&)>& mutate);
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorLevelMarkerEditingServiceContext context_;
};

} // namespace game
