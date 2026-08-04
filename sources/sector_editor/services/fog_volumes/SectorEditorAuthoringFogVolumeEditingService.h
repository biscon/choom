#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorAuthoringFogVolumeEditingServiceContext {
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

class SectorEditorAuthoringFogVolumeEditingService {
public:
    explicit SectorEditorAuthoringFogVolumeEditingService(
            SectorEditorAuthoringFogVolumeEditingServiceContext context);

    SectorAuthoringFogVolume* Selected();
    const SectorAuthoringFogVolume* Selected() const;
    bool Place(SectorTopologyCoordPoint point, int* outId = nullptr);
    bool MutateById(
            int fogVolumeId,
            const char* status,
            const std::function<bool(SectorAuthoringFogVolume&)>& mutate);
    bool SetPosition(int fogVolumeId, SectorTopologyCoordPoint point, const char* status);
    bool DeleteSelected();

    bool BeginMove(int fogVolumeId);
    void UpdateMove(SectorTopologyCoordPoint point);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);

    bool IsResolved(int fogVolumeId, int* outTopologySectorId = nullptr) const;
    int FindAtMapPoint(Vector2 mapPoint, float extraToleranceMap) const;

private:
    bool CanResolvePoint(SectorTopologyCoordPoint point, int* outTopologySectorId = nullptr) const;
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorAuthoringFogVolumeEditingServiceContext context_;
};

} // namespace game
