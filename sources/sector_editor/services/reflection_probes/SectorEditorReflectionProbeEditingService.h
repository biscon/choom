#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorReflectionProbeEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    ManipulationState& manipulationState;
    std::string& statusText;
};

class SectorEditorReflectionProbeEditingService {
public:
    explicit SectorEditorReflectionProbeEditingService(
            SectorEditorReflectionProbeEditingServiceContext context);

    SectorAuthoringReflectionProbe* Selected();
    const SectorAuthoringReflectionProbe* Selected() const;
    bool Place(SectorTopologyCoordPoint point, int* outId = nullptr);
    bool MutateById(
            int probeId,
            const char* status,
            const std::function<bool(SectorAuthoringReflectionProbe&)>& mutate);
    bool SetPosition(int probeId, SectorTopologyCoordPoint point, const char* status);
    bool FitToSector(int probeId);
    bool DeleteSelected();
    bool BeginMove(int probeId);
    void UpdateMove(SectorTopologyCoordPoint point);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    int FindAtMapPoint(Vector2 mapPoint, float toleranceMap) const;

private:
    bool CanResolvePoint(SectorTopologyCoordPoint point, int* outSectorId = nullptr) const;
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorReflectionProbeEditingServiceContext context_;
};

} // namespace game
