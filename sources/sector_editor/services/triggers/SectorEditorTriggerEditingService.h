#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/triggers/SectorEditorTriggerEditingState.h"

#include <functional>

namespace game {

bool SectorEditorTriggerHitTest(
        const SectorAuthoringTrigger& trigger,
        Vector2 mapPoint,
        float outlineToleranceMap);

struct SectorEditorTriggerEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    TriggerEditingState& editingState;
    std::string& statusText;
};

class SectorEditorTriggerEditingService {
public:
    explicit SectorEditorTriggerEditingService(SectorEditorTriggerEditingServiceContext context);

    SectorAuthoringTrigger* Selected();
    const SectorAuthoringTrigger* Selected() const;
    bool Place(SectorTriggerShapeKind shape, const std::vector<SectorTriggerPoint>& points, int* outId = nullptr);
    bool RenameSelected(const std::string& id, std::string& error);
    bool SetSelectedEnabled(bool enabled);
    bool SetSelectedRepeat(bool repeat);
    bool SetSelectedDelay(int milliseconds);
    bool SetSelectedScript(const std::string& script, std::string& error);
    bool DeleteSelected();

    bool BeginMove(int triggerId, SectorTriggerPoint pressPoint);
    void UpdateMove(SectorTriggerPoint point);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    bool IsMoving() const { return context_.editingState.drag.active; }
    int FindAtMapPoint(Vector2 mapPoint, float outlineToleranceMap) const;

private:
    bool MutateSelected(const char* status, const std::function<bool(SectorAuthoringTrigger&)>& mutate);
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);
    SectorEditorTriggerEditingServiceContext context_;
};

} // namespace game
