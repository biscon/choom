#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorAuthoringFaceMergeState {
    bool choosingTarget = false;
    int hoveredTargetFaceAnchorId = -1;
};

struct SectorEditorAuthoringFaceMergePlan {
    bool valid = false;
    int targetFaceAnchorId = -1;
    std::vector<int> selectedFaceAnchorIds;
    std::vector<int> removedAuthoringLineIds;
    std::vector<int> removedAuthoringVertexIds;
    int removedLineSideCount = 0;
    std::vector<int> removedDoorObjectIds;
    SectorAuthoringGraph candidateGraph;
    SectorAuthoringDerivationResult candidateDerivation;
    SectorTopologyMap candidateMapData;
    std::string status;
};

struct SectorEditorAuthoringFaceMergeServiceContext {
    SectorEditorState& state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState& selectionState;
    SectorEditorAuthoringFaceMergeState& mergeState;
    std::string& statusText;
};

class SectorEditorAuthoringFaceMergeService {
public:
    explicit SectorEditorAuthoringFaceMergeService(
            SectorEditorAuthoringFaceMergeServiceContext context);

    bool BeginTargetPick();
    void CancelTargetPick(const char* status = nullptr);
    bool IsChoosingTarget() const;
    int FindTargetAtMapPoint(Vector2 mapPoint, bool requireEligible = true) const;
    void UpdateTargetHover(Vector2 mapPoint);
    bool RequestMergeAtTarget(int targetFaceAnchorId);

    SectorEditorAuthoringFaceMergePlan BuildPlan(int targetFaceAnchorId) const;
    bool CommitPlan(SectorEditorAuthoringFaceMergePlan plan);

private:
    SectorEditorAuthoringFaceMergeServiceContext context_;
};

std::string BuildSectorEditorAuthoringFaceMergeConfirmationMessage(
        const SectorEditorAuthoringFaceMergePlan& plan,
        const SectorAuthoringGraph& graph);

} // namespace game
